# Writes Data/Audio/sounds.json: every event's files, volume, pitch, reach, category and gaps; ambient beds; music.
# Volumes are the mix (how loud each event should sit) times a loudness match: build_audio.py brings every short sound
# to the same loudness where its peaks allow, and what a very spiky file (a concrete footstep, a click) still lacks
# is made up here from its measured loudness, so all events start level before the mix is applied.
# Usage: python -I make_table.py <work>/audio_manifest.json Data/Audio/sounds.json
# (Or edit sounds.json by hand: it's the game's data; this only made the first version.)
import json, sys, math

MAN = json.load(open(sys.argv[1]))
OUT = sys.argv[2]

def files(*names):
    out = []
    for n in names:
        if n.endswith("*"):
            base = n[:-1]
            hits = sorted([k for k in MAN if k.startswith(base) and k[len(base):].isdigit()], key=lambda k: int(k[len(base):]))
            assert hits, n
            out += hits
        else:
            assert n in MAN, n
            out.append(n)
    return out

events = {}
def ev(name, fl, vol, radius_m, cat="effects", pitch=(0.95, 1.05), gap=0.05, voices=6, duck=0.0, match=True):
    target = -18.0 if cat == "ui" else -16.0
    if match:
        mean = sum(MAN[f]["lufs"] for f in fl) / len(fl)
        comp = min(3.5, max(0.5, 10 ** ((target - mean) / 20)))
    else:
        comp = 1.0
    e = {"files": fl, "volume": round(vol * comp, 2), "pitch": [pitch[0], pitch[1]], "radius": int(radius_m * 100), "category": cat, "minGap": gap}
    if voices != 6: e["maxVoices"] = voices
    if duck: e["duck"] = duck
    events[name] = e

# ---------------------------------------------------------------- player
for s in ["grass", "snow", "stone", "wood", "gravel", "sand", "water"]:
    ev(f"footstep.{s}", files(f"foot/{s}_*"), 0.3, 18, pitch=(0.9, 1.1), gap=0.12, voices=12)
ev("swing.light", files("combat/swing_light_*"), 0.45, 20, pitch=(0.92, 1.1), gap=0.08)
ev("swing.heavy", files("combat/swing_heavy_*"), 0.6, 22, pitch=(0.8, 0.92), gap=0.1)
ev("hit.flesh", files("combat/hit_flesh_*"), 0.8, 30, pitch=(0.9, 1.08), gap=0.03, voices=8)
ev("hit.armor", files("combat/hit_armor_*"), 0.75, 30, pitch=(0.92, 1.08), gap=0.03, voices=8)
ev("hit.block", files("combat/hit_block_*"), 0.85, 30, pitch=(0.9, 1.05), gap=0.05)
ev("hit.dust", files("world/gather_stone_*"), 0.7, 30, pitch=(0.75, 0.9), gap=0.03, voices=8)  # stone, dry or soft bodies (WRFx impacts)
ev("stomp", files("world/build_demolish_*"), 1.0, 60, pitch=(0.55, 0.7), gap=0.25)  # a boss or a big creature slams the ground
ev("parry", files("combat/parry_*"), 0.9, 35, pitch=(0.95, 1.08), gap=0.1)
# A creature winding up an attack (its telegraph, WRCreature.cpp): the heavy swish slowed to a deep whoomph, the same for
# every creature so the cue is learned once.
ev("creature.windup", files("combat/swing_heavy_*"), 0.7, 30, pitch=(0.5, 0.58), gap=0.25, voices=4)
ev("dodge", files("combat/dodge_*"), 0.5, 15, pitch=(0.9, 1.1), gap=0.3)
ev("jump", files("combat/jump_*"), 0.35, 12, pitch=(0.95, 1.1), gap=0.3)
ev("land", files("combat/land_*"), 0.5, 18, pitch=(0.85, 1.0), gap=0.3)
ev("hurt", files("voice/hurt_*"), 0.7, 25, pitch=(0.97, 1.03), gap=0.8, voices=4)
ev("hurt.female", files("voice/hurt_female_*"), 0.7, 25, pitch=(0.98, 1.04), gap=0.8, voices=4)
ev("death", files("voice/death_*"), 0.85, 35, pitch=(0.98, 1.02), gap=1.0)
ev("death.female", files("voice/death_female_*"), 0.85, 35, pitch=(0.98, 1.02), gap=1.0)
ev("eat", files("item/eat_*"), 0.6, 12, gap=0.6)
ev("drink", files("item/drink_*"), 0.6, 12, gap=0.6)
ev("potion", files("item/potion_*"), 0.7, 12, gap=0.6)
ev("pickup", files("item/pickup_*"), 0.55, 12, gap=0.15)
ev("equip", files("item/equip_*"), 0.55, 15, gap=0.2)
# ---------------------------------------------------------------- ranged
ev("bow.draw", files("ranged/bow_draw_*"), 0.4, 18, pitch=(0.95, 1.08), gap=0.3)
ev("bow.release", files("ranged/bow_release_*"), 0.7, 28, gap=0.1)
ev("crossbow.fire", files("ranged/crossbow_*"), 0.8, 35, pitch=(0.92, 1.05), gap=0.1)
ev("blowgun.fire", files("ranged/blowgun_*"), 0.6, 18, pitch=(1.0, 1.15), gap=0.1)
ev("throw", files("ranged/throw_*"), 0.6, 20, pitch=(0.8, 0.95), gap=0.1)
ev("arrow.hit", files("ranged/arrow_hit_*"), 0.7, 25, pitch=(0.92, 1.08), gap=0.03, voices=8)
# ---------------------------------------------------------------- spells
ev("cast.fire", files("spell/cast_fire_*"), 0.7, 35, gap=0.1)
ev("cast.frost", files("spell/cast_frost_*"), 0.75, 35, gap=0.1)
ev("fire.impact", files("spell/fire_impact_*"), 0.9, 50, pitch=(0.9, 1.05), gap=0.05)
ev("frost.impact", files("spell/frost_impact_*"), 0.85, 45, pitch=(0.92, 1.08), gap=0.05)
ev("firewall", files("spell/cast_fire_1", "spell/cast_fire_2"), 0.85, 45, pitch=(0.75, 0.85), gap=0.5)
ev("lavapool", files("spell/lavapool_*"), 0.75, 40, pitch=(0.85, 1.0), gap=0.5)
ev("frostnova", files("spell/cast_frost_1", "spell/frost_impact_1"), 0.95, 50, pitch=(0.8, 0.9), gap=0.5)
ev("icelance", files("spell/cast_frost_2", "spell/frost_impact_4"), 0.85, 40, pitch=(1.0, 1.12), gap=0.2)
# ---------------------------------------------------------------- world
ev("gather.wood", files("world/gather_wood_*"), 0.75, 30, pitch=(0.9, 1.08), gap=0.08)
ev("gather.stone", files("world/gather_stone_*"), 0.75, 30, pitch=(0.9, 1.08), gap=0.08)
ev("gather.fiber", files("foot/grass_2", "foot/grass_4", "combat/dodge_2", "combat/dodge_4"), 0.6, 18, pitch=(1.1, 1.25), gap=0.08)
ev("build.place", files("world/build_place_*"), 0.7, 25, pitch=(0.9, 1.05), gap=0.1)
ev("build.demolish", files("world/build_demolish_*"), 0.85, 40, pitch=(0.9, 1.05), gap=0.1)
ev("build.invalid", files("ui/error_1", "ui/error_2"), 0.55, 0, cat="ui", pitch=(0.98, 1.02), gap=0.25)
ev("door.open", files("world/door_open_*"), 0.7, 25, pitch=(0.95, 1.05), gap=0.3)
ev("door.close", files("world/door_close_*"), 0.7, 25, pitch=(0.95, 1.05), gap=0.3)
ev("craft.done", files("world/craft_done_*"), 0.65, 20, pitch=(0.97, 1.05), gap=0.3)
ev("hearth.feed", files("world/hearth_feed_*"), 0.75, 25, pitch=(0.95, 1.05), gap=0.5)
# ---------------------------------------------------------------- moments (2D; the music dips under them)
ev("levelup", files("moment/levelup"), 0.6, 0, pitch=(1, 1), gap=0.5, duck=0.6)
ev("recipe.unlock", files("moment/recipe_unlock"), 0.5, 0, pitch=(1, 1), gap=0.5, duck=0.4)
ev("discover", files("moment/discover"), 0.55, 0, pitch=(1, 1), gap=0.5, duck=0.5)
ev("tame.success", files("moment/tame_success"), 0.6, 0, pitch=(1, 1), gap=0.5, duck=0.5)
ev("boss.intro", files("moment/boss_intro"), 0.8, 0, pitch=(1, 1), gap=5.0, duck=0.85)
ev("respawn", files("moment/respawn"), 0.55, 0, pitch=(1, 1), gap=1.0, duck=0.4)
# ---------------------------------------------------------------- UI (2D)
ev("ui.hover", files("ui/hover_*"), 0.25, 0, cat="ui", pitch=(0.97, 1.03), gap=0.06, voices=3)
ev("ui.click", files("ui/click_2", "ui/click_3"), 0.5, 0, cat="ui", pitch=(0.97, 1.03), gap=0.04, voices=3)
ev("ui.open", files("ui/open_*"), 0.55, 0, cat="ui", pitch=(0.97, 1.03), gap=0.15, voices=2)
ev("ui.close", files("ui/close_*"), 0.5, 0, cat="ui", pitch=(0.97, 1.03), gap=0.15, voices=2)
ev("ui.error", files("ui/error_*"), 0.55, 0, cat="ui", pitch=(0.98, 1.02), gap=0.25, voices=2)
ev("ui.drag", files("ui/drag_*"), 0.45, 0, cat="ui", pitch=(0.97, 1.03), gap=0.08, voices=2)
ev("ui.drop", files("ui/drop_*"), 0.5, 0, cat="ui", pitch=(0.97, 1.03), gap=0.08, voices=2)
# ---------------------------------------------------------------- ambient spots
ev("amb.owl", files("amb/owl_*"), 0.45, 90, cat="ambient", pitch=(0.96, 1.04), gap=5.0, voices=1)

# ---------------------------------------------------------------- creatures: by family; per creature where it has its own
C = "creature/"
fam = {
    "canine": {"idle": ["canine_idle_*"], "aggro": ["canine_aggro_*"], "attack": ["canine_attack_*"], "hurt": ["canine_hurt_*"], "death": ["canine_death_*"]},
    "beast": {"idle": ["beast_idle_*"], "aggro": ["beast_aggro_*"], "attack": ["beast_attack_*"], "hurt": ["beast_hurt_*"], "death": ["beast_death_*"]},
    "flyer": {"idle": ["flyer_call_2", "flyer_call_3"], "aggro": ["flyer_call_1", "flyer_scream_1"], "attack": ["flyer_attack_1", "flyer_call_1"],
              "hurt": ["flyer_scream_2"], "death": ["flyer_scream_1", "flyer_scream_2"]},
    "bug": {"idle": ["bug_idle_*"], "aggro": ["bug_aggro_*"], "attack": ["bug_attack_*"], "hurt": ["bug_hurt_*"], "death": ["bug_death_*"]},
    "herd": {"idle": ["herd_idle_*"], "aggro": ["herd_aggro_*"], "attack": ["herd_attack_*"], "hurt": ["herd_hurt_*"], "death": ["herd_death_*"]},
    "critter": {"idle": ["critter_1", "critter_2", "critter_3", "critter_5"], "aggro": ["critter_6", "critter_7"], "attack": ["critter_8"],
                "hurt": ["critter_9", "critter_10"], "death": ["critter_4"]},
    "wisp": {"idle": ["wisp_chime_1", "wisp_chime_2", "wisp_chime_3"], "aggro": ["wisp_magic_1"], "attack": ["wisp_spell_*"], "hurt": ["wisp_chime_4", "wisp_chime_5"],
             "death": ["wisp_magic_1"]},
    "boss": {"idle": ["boss_idle_*"], "aggro": ["boss_roar_*"], "attack": ["boss_attack_*"], "hurt": ["boss_hurt_*"], "death": ["boss_roar_1"]},
    "golem": {"idle": ["golem_stones_1", "golem_stones_3"], "aggro": ["golem_roar_*"], "attack": ["golem_stones_2", "golem_stones_4"], "hurt": ["golem_stones_2", "golem_stones_4"],
              "death": ["golem_death_1"]},
    "humanoid": {"aggro": ["human_5", "human_6"], "attack": ["human_1", "human_2", "human_3"], "hurt": ["human_4", "human_7"], "death": ["human_8"]},
}
reach = {"idle": 30, "aggro": 45, "attack": 30, "hurt": 30, "death": 40}
vol = {"idle": 0.55, "aggro": 0.85, "attack": 0.75, "hurt": 0.75, "death": 0.85}
gaps = {"idle": 2.0, "aggro": 5.0, "attack": 0.25, "hurt": 0.3, "death": 1.0}
for f, kinds in fam.items():
    big = f in ("boss", "golem")
    for k, fl in kinds.items():
        ev(f"creature.{f}.{k}", files(*[C + x for x in fl]), vol[k], reach[k] * (2.0 if big else 1.0), pitch=(0.94, 1.06), gap=gaps[k], voices=4)
pig = {"idle": ["pig_1", "pig_2", "pig_4"], "hurt": ["pig_3"], "death": ["pig_5"], "aggro": ["pig_3"]}
for k, fl in pig.items():
    ev(f"creature.pumicehog.{k}", files(*[C + x for x in fl]), vol[k], reach[k], pitch=(0.95, 1.08), gap=gaps[k], voices=3)

creatures = {
    "cinderfang": ["canine", 1.12], "gloomhowl": ["canine", 0.9], "magmaw": ["beast", 0.85], "rimeclaw": ["beast", 1.0],
    "scorchwing": ["flyer", 0.9], "basaltback": ["herd", 0.7], "woolly_drudge": ["herd", 0.78], "emberhorn": ["herd", 1.05],
    "frostantler": ["herd", 1.15], "ashstrider": ["herd", 1.3], "kilnmite": ["bug", 1.15], "glacimaw": ["bug", 0.75],
    "rimeshell": ["bug", 0.9], "glowmoth": ["critter", 1.5], "pumicehog": ["critter", 1.0], "snowpuff": ["critter", 1.35],
    "aurora_fox": ["critter", 1.2], "wisp": ["wisp", 1.0], "vulkarn": ["boss", 0.85], "skaldrith": ["boss", 0.75],
    "obsidian_warden": ["golem", 0.9], "ignessa": ["wisp", 0.75], "grimjaw": ["boss", 1.0], "pale_hunter": ["humanoid", 0.9],
}

table = {
    "_comment": "Wildreach sound table (WRSound.h has the event names; files are .ogg under Data/Audio, names without the extension). "
                "volume: 0..4 (the loudness match is already in it), pitch: [min, max], radius: cm (0 = 2D), category: effects / ui / ambient / music, "
                "minGap: seconds before the same event plays again on the same actor or spot, maxVoices: at once, duck: how far music dips under it. "
                "creatures: voice family and pitch per creature id. ambient: beds crossfaded by layer (volcanic, tundra, coast, day, night, "
                "volcanic.day/night, tundra.day/night, cave, lava, high, wind, always). music: pools (menu, explore, night, combat, boss) and timings in seconds. "
                "Rebuilt from the sources by Tools/audio (see Docs/CREDITS.md).",
    "mix": {"effects": 1.0, "ui": 0.6, "ambient": 1.0, "music": 0.7},
    "defaults": {"radius": 2500, "pitch": [0.96, 1.04], "minGap": 0.05, "maxVoices": 6},
    "events": dict(sorted(events.items())),
    "creatures": {k: {"family": v[0], "pitch": v[1]} for k, v in creatures.items()},
    "ambient": {
        "fadeSeconds": 3.0, "caveFade": 1.5, "biomeBlend": 8000, "highFrom": 25000, "highTo": 45000, "lavaReach": 3500,
        "watchRange": 4500, "idleRange": 3000, "idleEvery": [12, 35], "owlEvery": [25, 70],
        "beds": [
            {"id": "volcanic.rumble", "file": "amb/volcanic_rumble", "volume": 0.5, "layer": "volcanic"},
            {"id": "volcanic.crackle", "file": "amb/volcanic_crackle", "volume": 0.6, "layer": "volcanic"},
            {"id": "lava", "file": "amb/lava", "volume": 0.75, "layer": "lava"},
            {"id": "tundra.wind", "file": "amb/tundra_wind", "volume": 0.65, "layer": "tundra"},
            {"id": "high.wind", "file": "amb/high_wind", "volume": 0.45, "layer": "high"},
            {"id": "coast.surf", "file": "amb/coast_surf", "volume": 0.65, "layer": "coast"},
            {"id": "day.birds", "file": "amb/day_birds", "volume": 0.4, "layer": "day"},
            {"id": "night.insects", "file": "amb/night_insects", "volume": 0.4, "layer": "volcanic.night"},
            {"id": "night.wind", "file": "amb/high_wind", "volume": 0.3, "layer": "tundra.night"},
            {"id": "cave", "file": "amb/cave", "volume": 0.7, "layer": "cave"},
        ],
    },
    "music": {
        "fadeIn": 4.0, "fadeOut": 4.0, "combatFadeIn": 1.2, "combatFadeOut": 3.0, "combatHold": 8.0, "combatRange": 5000, "bossRange": 4500,
        "firstGap": [25, 50], "gap": [120, 300], "afterCombatGap": [40, 90], "exploreMaxSeconds": 200, "nightBelow": 0.35,
        "pools": {
            "menu": [{"file": "music/menu", "volume": 0.9}],
            "explore": [{"file": "music/explore_winter", "volume": 0.85}, {"file": "music/explore_winds", "volume": 0.85}, {"file": "music/explore_lands", "volume": 0.8}],
            "night": [{"file": "music/night_moon", "volume": 0.8}, {"file": "music/night_cathedral", "volume": 0.75}],
            "combat": [{"file": "music/combat", "volume": 0.8}],
            "boss": [{"file": "music/boss", "volume": 0.85}],
        },
    },
}

# One event / bed / track per line: easy to read and to edit by hand.
def dump(o, ind=0):
    pad = "\t" * ind
    if isinstance(o, dict) and any(isinstance(v, (dict, list)) and len(json.dumps(v)) > 140 for v in o.values()):
        items = [f'{pad}\t{json.dumps(k)}: {dump(v, ind + 1).lstrip()}' for k, v in o.items()]
        return "{\n" + ",\n".join(items) + f"\n{pad}}}"
    if isinstance(o, list) and len(json.dumps(o)) > 140 and all(isinstance(v, dict) for v in o):
        return "[\n" + ",\n".join(f"{pad}\t{json.dumps(v)}" for v in o) + f"\n{pad}]"
    return json.dumps(o)
open(OUT, "w", encoding="utf-8", newline="\n").write(dump(table) + "\n")
used = set(f for e in events.values() for f in e["files"]) | {b["file"] for b in table["ambient"]["beds"]} | \
       {t["file"] for p in table["music"]["pools"].values() for t in p}
print(len(events), "events;", len(used), "files used of", len(MAN), "; unused:", sorted(set(MAN) - used))
