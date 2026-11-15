"""Adds the Storm peaks landscape (cooks/build-biome-lightning.md 2.1) to /Game/Maps/L_Island, beside the island's.

Inputs (from Tools/terrain/storm_peaks.mjs): Art/Terrain/storm_height.r16 (2017 x 2017 samples, 2 m apart, value =
32768 + metres * 16) and Data/storm_peaks.json (frame, camps). Places the landscape at its frame corner, three
PlayerStarts tagged storm_peaks at the camps, their safe zones and the middle camp's starter stations. Re-running
replaces what an earlier run added (actors labelled Storm_* / Landscape_StormPeaks). Runs in the full editor,
unattended (the landscape's edit layers render on the GPU during the editor's tick): Tools/terrain/build_storm.ps1
"""
import json
import os
import struct

import unreal

MAP_PATH = "/Game/Maps/L_Island"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
HEIGHTMAP = os.path.join(PROJECT, "Art", "Terrain", "storm_height.r16")
DATA = json.load(open(os.path.join(PROJECT, "Data", "storm_peaks.json"), encoding="utf-8"))
F = DATA["frame"]
SIZE, STEP, UNITS = F["samples"], F["step"], F["unitsPerMetre"]
Z_SCALE = 100.0 * 128.0 / UNITS
RAW = open(HEIGHTMAP, "rb").read()
assert len(RAW) == SIZE * SIZE * 2, "storm heightmap has the wrong size"


def log(msg):
    unreal.log(f"[Wildreach] {msg}")


def height_m(x_m, y_m):
    i = max(0, min(SIZE - 1, int(round((x_m - F["x0"]) / STEP))))
    j = max(0, min(SIZE - 1, int(round((y_m - F["y0"]) / STEP))))
    (v,) = struct.unpack_from("<H", RAW, (j * SIZE + i) * 2)
    return (v - 32768) / UNITS


def world(x_m, y_m, lift_cm=0.0):
    return unreal.Vector(x_m * 100.0, y_m * 100.0, height_m(x_m, y_m) * 100.0 + lift_cm)


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not les.load_level(MAP_PATH):
    raise RuntimeError("could not load " + MAP_PATH)

gone = 0
for a in actors.get_all_level_actors():
    label = a.get_actor_label()
    # The old landscape's streaming proxies too: every proxy whose corner lies north of the island's frame is Storm
    # peaks' (the island's start at y = -201600 cm).
    # Only Storm peaks' own proxies: they sit on its 504 m grid from its corner (the Swamp's landscape, north of the
    # island too, has its own grid: import_swamp.py).
    loc = a.get_actor_location()
    on_grid = abs(((loc.x - F["x0"] * 100.0) + 1.0) % 50400.0 - 1.0) < 2.0 and loc.x < (F["x0"] + (SIZE - 1) * STEP) * 100.0
    is_storm_proxy = isinstance(a, unreal.LandscapeStreamingProxy) and not isinstance(a, unreal.Landscape) and loc.y < -201600.0 - 1.0 and on_grid
    if label.startswith("Storm_") or label.startswith("Landscape_StormPeaks") or is_storm_proxy:
        actors.destroy_actor(a)
        gone += 1
log(f"removed {gone} actors from an earlier Storm peaks import")

mat = unreal.load_asset("/Game/Terrain/M_StormAuto")
if not mat:
    raise RuntimeError("M_StormAuto missing: run build_terrain_materials.py with WR_MATERIALS=storm first")
before = set(a.get_name() for a in actors.get_all_level_actors() if isinstance(a, unreal.Landscape))
ok = unreal.WREditorTerrainLibrary.import_landscape_r16(
    HEIGHTMAP, SIZE, unreal.Vector(F["x0"] * 100.0, F["y0"] * 100.0, 0.0),
    unreal.Vector(STEP * 100.0, STEP * 100.0, Z_SCALE), 63, 2, 2, mat)
if not ok:
    raise RuntimeError("storm landscape import failed")
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.Landscape) and a.get_name() not in before:
        a.set_actor_label("Landscape_StormPeaks")
log("storm landscape imported")


def spawn(cls, loc, rot=(0, 0, 0), label=None):
    a = actors.spawn_actor_from_class(cls, loc, unreal.Rotator(*rot))
    if label:
        a.set_actor_label(label)
    return a


# The three camps: PlayerStarts for the Lightning path, a safe zone each, starter stations at the middle camp
# (inland of it: the camps look west over their coves).
safe_class = unreal.load_class(None, "/Script/Wildreach.WRSafeZone")
station_class = unreal.load_class(None, "/Script/Wildreach.WRStation")
camps = [s for s in DATA["sites"] if s["kind"] == "camp"]
for n, c in enumerate(camps):
    ps = spawn(unreal.PlayerStart, world(c["x"], c["y"], 200), (0, 0, 180), f"Storm_Spawn_{n}")
    ps.set_editor_property("player_start_tag", "storm_peaks")
    z = spawn(safe_class, world(c["x"], c["y"], 100), label=f"Storm_SafeZone_{n}")
    z.set_editor_property("radius", 4000.0)
    if n == 1:
        for k, station in enumerate(["forge", "armorer", "alchemy_table", "workbench"]):
            st = spawn(station_class, world(c["x"] + 25, c["y"] - 12 + k * 8, 40), label=f"Storm_Camp_{station}")
            st.set_editor_property("station_type", station)
log(f"placed {len(camps)} Storm camps")

state = {"ticks": 0, "handle": None}


def finish_when_ready(delta):
    state["ticks"] += 1
    ready = unreal.WREditorTerrainLibrary.are_landscapes_up_to_date()
    if not ready and state["ticks"] < 4000:
        if state["ticks"] % 200 == 0:
            log(f"waiting for landscape layers ({state['ticks']} ticks)")
        return
    unreal.unregister_slate_post_tick_callback(state["handle"])
    log(f"landscape layers {'up to date' if ready else 'NOT up to date (timed out)'} after {state['ticks']} ticks")
    if not les.save_current_level():
        unreal.log_error("[Wildreach] save_current_level failed")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("saved L_Island with Storm peaks")
    unreal.SystemLibrary.quit_editor()


state["handle"] = unreal.register_slate_post_tick_callback(finish_when_ready)
