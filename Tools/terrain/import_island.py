"""Builds /Game/Maps/L_Island: the approved first-playable landmass as a World Partition landscape.

Inputs (from Tools/terrain/layout_proposal.mjs):
  Art/Terrain/island_height.r16      4033 x 4033, value = 32768 + metres * 32
  Docs/terrain/layout_proposal.json  spawns, passes, boss and mini-boss sites (metres)
Run headless: Tools/terrain/build_island.ps1 [-Force]
"""
import json
import os
import random
import struct

import unreal

MAP_PATH = "/Game/Maps/L_Island"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
HEIGHTMAP = os.path.join(PROJECT, "Art", "Terrain", "island_height.r16")
LAYOUT = os.path.join(PROJECT, "Docs", "terrain", "layout_proposal.json")

SIZE = 4033                 # samples per side, 1 m apart
UNITS_PER_METRE = 32        # heightmap encoding
Z_SCALE = 100.0 * 128.0 / UNITS_PER_METRE / 100.0 * 100.0  # 400: landscape Z scale for that encoding
HALF = (SIZE - 1) * 100.0 / 2.0  # landscape centred on the origin

def log(msg):
    unreal.log(f"[Wildreach] {msg}")

# ---------------------------------------------------------------- heightmap access
with open(HEIGHTMAP, "rb") as f:
    RAW = f.read()
assert len(RAW) == SIZE * SIZE * 2, "heightmap has the wrong size"

def height_m(x_m, y_m):
    i = max(0, min(SIZE - 1, int(round(x_m))))
    j = max(0, min(SIZE - 1, int(round(y_m))))
    (v,) = struct.unpack_from("<H", RAW, (j * SIZE + i) * 2)
    return (v - 32768) / UNITS_PER_METRE

def slope(x_m, y_m):
    return max(abs(height_m(x_m + 2, y_m) - height_m(x_m - 2, y_m)), abs(height_m(x_m, y_m + 2) - height_m(x_m, y_m - 2))) / 4.0

def world(x_m, y_m, lift_cm=0.0):
    return unreal.Vector(x_m * 100.0 - HALF, y_m * 100.0 - HALF, height_m(x_m, y_m) * 100.0 + lift_cm)

# ---------------------------------------------------------------- materials
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

def get_or_create(path, cls, factory):
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit("/", 1)
    return asset_tools.create_asset(name, folder, cls, factory)

def build_landscape_material():
    path = "/Game/Terrain/M_IslandAuto"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mat = get_or_create(path, unreal.Material, unreal.MaterialFactoryNew())

    def expr(cls, x, y):
        return mel.create_material_expression(mat, cls, x, y)

    def const3(rgb, x, y):
        e = expr(unreal.MaterialExpressionConstant3Vector, x, y)
        e.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
        return e

    def scalar(v, x, y):
        e = expr(unreal.MaterialExpressionConstant, x, y)
        e.set_editor_property("r", v)
        return e

    # Biome by world X (ridge runs near X = 0): ash west, snow east, ~40 m blend.
    wpos = expr(unreal.MaterialExpressionWorldPosition, -1400, 0)
    mask_x = expr(unreal.MaterialExpressionComponentMask, -1200, 0)
    mask_x.set_editor_property("r", True)
    mask_x.set_editor_property("g", False)
    mask_x.set_editor_property("b", False)
    mel.connect_material_expressions(wpos, "", mask_x, "")
    div = expr(unreal.MaterialExpressionDivide, -1000, 0)
    mel.connect_material_expressions(mask_x, "", div, "A")
    mel.connect_material_expressions(scalar(8000.0, -1200, 120), "", div, "B")
    add = expr(unreal.MaterialExpressionAdd, -850, 0)
    mel.connect_material_expressions(div, "", add, "A")
    mel.connect_material_expressions(scalar(0.5, -1000, 120), "", add, "B")
    biome = expr(unreal.MaterialExpressionSaturate, -700, 0)
    mel.connect_material_expressions(add, "", biome, "")

    base = expr(unreal.MaterialExpressionLinearInterpolate, -500, -100)
    mel.connect_material_expressions(const3((0.028, 0.024, 0.022), -700, -250), "", base, "A")  # volcanic ash/basalt
    mel.connect_material_expressions(const3((0.78, 0.82, 0.88), -700, -150), "", base, "B")     # tundra snow
    mel.connect_material_expressions(biome, "", base, "Alpha")

    # Steep slopes show rock.
    normal = expr(unreal.MaterialExpressionVertexNormalWS, -1400, 300)
    mask_z = expr(unreal.MaterialExpressionComponentMask, -1200, 300)
    mask_z.set_editor_property("r", False)
    mask_z.set_editor_property("g", False)
    mask_z.set_editor_property("b", True)
    mel.connect_material_expressions(normal, "", mask_z, "")
    sub = expr(unreal.MaterialExpressionSubtract, -1000, 300)
    mel.connect_material_expressions(scalar(0.9, -1200, 420), "", sub, "A")
    mel.connect_material_expressions(mask_z, "", sub, "B")
    mul = expr(unreal.MaterialExpressionMultiply, -850, 300)
    mel.connect_material_expressions(sub, "", mul, "A")
    mel.connect_material_expressions(scalar(12.0, -1000, 420), "", mul, "B")
    rock_alpha = expr(unreal.MaterialExpressionSaturate, -700, 300)
    mel.connect_material_expressions(mul, "", rock_alpha, "")

    final = expr(unreal.MaterialExpressionLinearInterpolate, -300, 0)
    mel.connect_material_expressions(base, "", final, "A")
    mel.connect_material_expressions(const3((0.11, 0.105, 0.1), -500, 150), "", final, "B")     # rock
    mel.connect_material_expressions(rock_alpha, "", final, "Alpha")

    mel.connect_material_property(final, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar(0.92, -300, 200), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat

def colour_instance(path, rgb):
    mic = get_or_create(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mic.set_editor_property("parent", unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial"))
    mel.set_material_instance_vector_parameter_value(mic, "Color", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    eal.save_loaded_asset(mic)
    return mic

# ---------------------------------------------------------------- build
if eal.does_asset_exist(MAP_PATH):
    unreal.log_warning(f"[Wildreach] {MAP_PATH} already exists (build_island.ps1 -Force rebuilds it)")
    unreal.SystemLibrary.quit_editor()
else:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not les.new_level(MAP_PATH, True):
        raise RuntimeError("new_level failed")

    layout = json.load(open(LAYOUT))
    land_mat = build_landscape_material()
    sea_mat = colour_instance("/Game/Terrain/MI_Sea", (0.03, 0.12, 0.25))

    ok = unreal.WREditorTerrainLibrary.import_landscape_r16(
        HEIGHTMAP, SIZE, unreal.Vector(-HALF, -HALF, 0.0), unreal.Vector(100.0, 100.0, Z_SCALE), 63, 2, 2, land_mat)
    if not ok:
        raise RuntimeError("landscape import failed")
    log("landscape imported")

    def spawn(cls, loc, rot=(0, 0, 0), label=None):
        a = actors.spawn_actor_from_class(cls, loc, unreal.Rotator(*rot))
        if label:
            a.set_actor_label(label)
        return a

    # Sea, sky, light.
    plane = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    sea = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, -20), label="Sea")
    sea.static_mesh_component.set_static_mesh(plane)
    sea.static_mesh_component.set_material(0, sea_mat)
    sea.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    sea.set_actor_scale3d(unreal.Vector(1200.0, 1200.0, 1.0))
    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 100000), (0, -40, 35), "Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 50000), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), label="HeightFog")
    spawn(unreal.VolumetricCloud, unreal.Vector(0, 0, 0), label="Clouds")

    # Spawns (player starts tagged by biome).
    for i, s in enumerate(layout["spawns"]):
        yaw = 0 if s["biome"] == "volcanic" else 180
        ps = spawn(unreal.PlayerStart, world(s["x"], s["y"], 200), (0, 0, yaw), f"Spawn_{s['biome']}_{i}")
        ps.set_editor_property("player_start_tag", s["biome"])

    # Boss altars at the arena sites (bosses themselves are M6).
    altar_class = unreal.load_class(None, "/Script/Wildreach.WRBossAltar")
    for b, path_id in zip(layout["bosses"], ["fire", "frost"]):
        a = spawn(altar_class, world(b["x"] + 40, b["y"], 50), label=f"Altar_{path_id}")
        a.set_editor_property("path_id", path_id)

    # Creature packs by zone: lowland packs near the spawns, midland predators toward the ridge.
    spawner_class = unreal.load_class(None, "/Script/Wildreach.WRCreatureSpawner")
    packs = [("cinderfang", 950, 1100), ("cinderfang", 1000, 2900), ("magmaw", 1450, 1700),
             ("gloomhowl", 3080, 1100), ("gloomhowl", 3050, 2950), ("rimeclaw", 2600, 2450)]
    for creature, x, y in packs:
        sp = spawn(spawner_class, world(x, y, 100), label=f"Pack_{creature}_{x}_{y}")
        sp.set_editor_property("creature_id", creature)
        sp.set_editor_property("count", 1 if creature in ("magmaw", "rimeclaw") else 3)
        sp.set_editor_property("radius", 500.0)
        sp.set_editor_property("respawn_seconds", 60.0)

    # Resource nodes scattered on walkable land, biome-appropriate.
    node_class = unreal.load_class(None, "/Script/Wildreach.WRResourceNode")
    rng = random.Random(7)
    ridge_x = 2016
    counts = {"wood": 40, "stone": 40, "copper_ore": 12, "sulfur_crystal": 16, "lichen": 16}
    placed = 0
    for item, count in counts.items():
        n = 0
        tries = 0
        while n < count and tries < count * 60:
            tries += 1
            x, y = rng.uniform(300, 3730), rng.uniform(300, 3730)
            west = x < ridge_x
            if item == "sulfur_crystal" and not west:
                continue
            if item == "lichen" and west:
                continue
            if height_m(x, y) < 3 or slope(x, y) > 0.5 or abs(x - ridge_x) < 450:
                continue
            node = spawn(node_class, world(x, y, 0), (0, rng.uniform(0, 360), 0), f"Node_{item}_{n}")
            node.set_editor_property("item_id", item)
            n += 1
        placed += n
    log(f"placed {placed} resource nodes")

    # The landscape's visible textures are produced by the edit-layer update during the editor's
    # tick, so save and quit from a tick callback once it reports up to date.
    state = {"ticks": 0, "handle": None}

    def finish_when_ready(delta):
        state["ticks"] += 1
        ready = unreal.WREditorTerrainLibrary.are_landscapes_up_to_date()
        if not ready and state["ticks"] < 3000:
            if state["ticks"] % 200 == 0:
                log(f"waiting for landscape layers ({state['ticks']} ticks)")
            return
        unreal.unregister_slate_post_tick_callback(state["handle"])
        log(f"landscape layers {'up to date' if ready else 'NOT up to date (timed out)'} after {state['ticks']} ticks")
        if not les.save_current_level():
            unreal.log_error("[Wildreach] save_current_level failed")
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        log(f"created {MAP_PATH}")
        unreal.SystemLibrary.quit_editor()

    state["handle"] = unreal.register_slate_post_tick_callback(finish_when_ready)
