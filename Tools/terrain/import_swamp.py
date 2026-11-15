"""Adds the Swamp landscape (cooks/build-biome-swamp.md 2.1) to /Game/Maps/L_Island, beside the island's and Storm peaks'.

Inputs (from Tools/terrain/swamp.mjs): Art/Terrain/swamp_height.r16 (2017 x 2017 samples, 2 m apart, value =
32768 + metres * 16) and Data/swamp.json (frame, camps). Places the landscape at its frame corner, three PlayerStarts
tagged swamp at the camps, their safe zones and the middle camp's starter stations. Re-running replaces what an earlier
run added (actors labelled Swamp_* / Landscape_Swamp, and the streaming proxies on the Swamp's own 504 m grid). Runs in
the full editor, unattended (the landscape's edit layers render on the GPU during the editor's tick):
Tools/terrain/build_swamp.ps1
"""
import json
import os
import struct

import unreal

MAP_PATH = "/Game/Maps/L_Island"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
HEIGHTMAP = os.path.join(PROJECT, "Art", "Terrain", "swamp_height.r16")
DATA = json.load(open(os.path.join(PROJECT, "Data", "swamp.json"), encoding="utf-8"))
F = DATA["frame"]
SIZE, STEP, UNITS = F["samples"], F["step"], F["unitsPerMetre"]
Z_SCALE = 100.0 * 128.0 / UNITS
GRID_CM = 50400.0   # 2 x 2 components of 126 quads, 2 m apart: one streaming proxy
RAW = open(HEIGHTMAP, "rb").read()
assert len(RAW) == SIZE * SIZE * 2, "swamp heightmap has the wrong size"


def log(msg):
    unreal.log(f"[Wildreach] {msg}")


def height_m(x_m, y_m):
    i = max(0, min(SIZE - 1, int(round((x_m - F["x0"]) / STEP))))
    j = max(0, min(SIZE - 1, int(round((y_m - F["y0"]) / STEP))))
    (v,) = struct.unpack_from("<H", RAW, (j * SIZE + i) * 2)
    return (v - 32768) / UNITS


def world(x_m, y_m, lift_cm=0.0):
    return unreal.Vector(x_m * 100.0, y_m * 100.0, height_m(x_m, y_m) * 100.0 + lift_cm)


def on_swamp_grid(loc):
    x0, y0 = F["x0"] * 100.0, F["y0"] * 100.0
    x1, y1 = (F["x0"] + (SIZE - 1) * STEP) * 100.0, (F["y0"] + (SIZE - 1) * STEP) * 100.0
    if not (x0 - 1.0 <= loc.x <= x1 and y0 - 1.0 <= loc.y <= y1):
        return False
    return abs(((loc.x - x0) + 1.0) % GRID_CM - 1.0) < 2.0 and abs(((loc.y - y0) + 1.0) % GRID_CM - 1.0) < 2.0


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not les.load_level(MAP_PATH):
    raise RuntimeError("could not load " + MAP_PATH)

gone = 0
for a in actors.get_all_level_actors():
    label = a.get_actor_label()
    is_proxy = isinstance(a, unreal.LandscapeStreamingProxy) and not isinstance(a, unreal.Landscape) and on_swamp_grid(a.get_actor_location())
    if label.startswith("Swamp_") or label.startswith("Landscape_Swamp") or is_proxy:
        actors.destroy_actor(a)
        gone += 1
log(f"removed {gone} actors from an earlier Swamp import")

mat = unreal.load_asset("/Game/Terrain/M_SwampAuto")
if not mat:
    raise RuntimeError("M_SwampAuto missing: run build_terrain_materials.py with WR_MATERIALS=swamp first")
before = set(a.get_name() for a in actors.get_all_level_actors() if isinstance(a, unreal.Landscape))
ok = unreal.WREditorTerrainLibrary.import_landscape_r16(
    HEIGHTMAP, SIZE, unreal.Vector(F["x0"] * 100.0, F["y0"] * 100.0, 0.0),
    unreal.Vector(STEP * 100.0, STEP * 100.0, Z_SCALE), 63, 2, 2, mat)
if not ok:
    raise RuntimeError("swamp landscape import failed")
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.Landscape) and a.get_name() not in before:
        a.set_actor_label("Landscape_Swamp")
log("swamp landscape imported")


def spawn(cls, loc, rot=(0, 0, 0), label=None):
    a = actors.spawn_actor_from_class(cls, loc, unreal.Rotator(*rot))
    if label:
        a.set_actor_label(label)
    return a


# The three camps (world-map 2.2): PlayerStarts for the Blood path, a safe zone each, starter stations at the middle
# camp (inland of it: the camps look east over the reed flats to the sea).
safe_class = unreal.load_class(None, "/Script/Wildreach.WRSafeZone")
station_class = unreal.load_class(None, "/Script/Wildreach.WRStation")
camps = DATA["camps"]
for n, c in enumerate(camps):
    ps = spawn(unreal.PlayerStart, world(c["x"], c["y"], 200), (0, 0, 0), f"Swamp_Spawn_{n}")
    ps.set_editor_property("player_start_tag", "swamp")
    z = spawn(safe_class, world(c["x"], c["y"], 100), label=f"Swamp_SafeZone_{n}")
    z.set_editor_property("radius", 4000.0)
    if n == 1:
        for k, station in enumerate(["forge", "armorer", "alchemy_table", "workbench"]):
            st = spawn(station_class, world(c["x"] - 25, c["y"] - 12 + k * 8, 40), label=f"Swamp_Camp_{station}")
            st.set_editor_property("station_type", station)
log(f"placed {len(camps)} Swamp camps")

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
    log("saved L_Island with the Swamp")
    unreal.SystemLibrary.quit_editor()


state["handle"] = unreal.register_slate_post_tick_callback(finish_when_ready)
