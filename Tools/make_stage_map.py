"""Creates /Game/Maps/L_CreatureStage: a small stage for creature animation captures (Tools/creaturevideo.ps1).
A 400 m flat floor (a 1 m grey checker, M_CreatureStage) and, beside the path along +X (y = 0), a 20 degree ramp whose foot runs along y = -200
from x = -10 m to +50 m, rising toward -Y for 30 m (10 m high at the top; a sandy checker, M_CreatureStageRamp). Sun, sky, sky light and fog like the
combat arena; the game's sky (WRSky) takes the sun over and moves it with the clock. No spawners, no props.
Re-run with -Force to rebuild: Tools/make_map.ps1 stage -Force
"""
import math
import os
import unreal

MAP_PATH = "/Game/Maps/L_CreatureStage"
FORCE = os.environ.get("WR_FORCE") == "1"
SLOPE_DEG = 20.0
FOOT_Y = -200.0           # cm: where the ramp meets the floor (the path runs along y = 0)
X0, X1 = -1000.0, 5000.0  # cm: the ramp's extent along the path
UP_SLOPE = 3000.0         # cm of ramp above the floor, measured along its surface
UNDER = 200.0             # cm of ramp surface that dives under the floor past its foot
THICK = 2000.0            # cm: the slab is deep enough that its underside never shows

MATERIAL_DIR = "/Game/Maps"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def stage_material(name, dark_rgb, light_rgb):
    """A 1 m checker laid in world X/Y (the same squares on the floor and, seen from above, on the ramp), so a sliding
    foot shows against it."""
    path = f"{MATERIAL_DIR}/{name}"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        n = mel.create_material_expression(mat, cls, x, y)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    world = node(unreal.MaterialExpressionWorldPosition, -1200, 0)
    xy = node(unreal.MaterialExpressionComponentMask, -1000, 0, r=True, g=True, b=False, a=False)
    mel.connect_material_expressions(world, "", xy, "")
    metres = node(unreal.MaterialExpressionDivide, -850, 0, const_b=100.0)
    mel.connect_material_expressions(xy, "", metres, "A")
    cell = node(unreal.MaterialExpressionFloor, -700, 0)
    mel.connect_material_expressions(metres, "", cell, "")
    ones = node(unreal.MaterialExpressionConstant2Vector, -700, 150, r=1.0, g=1.0)
    total = node(unreal.MaterialExpressionDotProduct, -550, 0)
    mel.connect_material_expressions(cell, "", total, "A")
    mel.connect_material_expressions(ones, "", total, "B")
    half = node(unreal.MaterialExpressionMultiply, -400, 0, const_b=0.5)
    mel.connect_material_expressions(total, "", half, "A")
    frac = node(unreal.MaterialExpressionFrac, -300, 0)
    mel.connect_material_expressions(half, "", frac, "")
    odd = node(unreal.MaterialExpressionMultiply, -200, 0, const_b=2.0)  # 0 on even squares, 1 on odd
    mel.connect_material_expressions(frac, "", odd, "A")
    dark = node(unreal.MaterialExpressionConstant3Vector, -250, 150, constant=unreal.LinearColor(*dark_rgb, 1.0))
    light = node(unreal.MaterialExpressionConstant3Vector, -250, 300, constant=unreal.LinearColor(*light_rgb, 1.0))
    pick = node(unreal.MaterialExpressionLinearInterpolate, -50, 100)
    mel.connect_material_expressions(dark, "", pick, "A")
    mel.connect_material_expressions(light, "", pick, "B")
    mel.connect_material_expressions(odd, "", pick, "Alpha")
    mel.connect_material_property(pick, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(unreal.MaterialExpressionConstant, -50, 300, r=0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[Wildreach] created {path}")
    return mat


if eal.does_asset_exist(MAP_PATH) and FORCE:
    eal.delete_asset(MAP_PATH)

if eal.does_asset_exist(MAP_PATH):
    unreal.log_warning(f"[Wildreach] {MAP_PATH} already exists (pass -force to rebuild)")
else:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not les.new_level(MAP_PATH, False):
        raise RuntimeError("[Wildreach] new_level failed")

    def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
        # rot is (roll, pitch, yaw), as unreal.Rotator takes them
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
        if label:
            actor.set_actor_label(label)
        return actor

    plane = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    grid = stage_material("M_CreatureStage", (0.20, 0.20, 0.19), (0.36, 0.36, 0.34))
    ramp_grid = stage_material("M_CreatureStageRamp", (0.22, 0.16, 0.10), (0.40, 0.31, 0.21))  # sandier: the ramp reads apart

    floor = spawn(unreal.StaticMeshActor, label="Stage_Floor")
    floor.static_mesh_component.set_static_mesh(plane)
    floor.set_actor_scale3d(unreal.Vector(400.0, 400.0, 1.0))  # 400 m square
    if grid:
        floor.static_mesh_component.set_material(0, grid)

    # The ramp: a slab rolled about X so its top face rises toward -Y. Its top face meets the floor along y = FOOT_Y.
    length_along = UP_SLOPE + UNDER
    ramp = spawn(unreal.StaticMeshActor, label="Stage_Ramp")
    ramp.static_mesh_component.set_static_mesh(cube)
    ramp.set_actor_scale3d(unreal.Vector((X1 - X0) / 100.0, length_along / 100.0, THICK / 100.0))
    # Pick the roll whose slab "right" (local +Y) points down toward +Y: then local -Y climbs toward world -Y.
    for roll in (SLOPE_DEG, -SLOPE_DEG):
        ramp.set_actor_rotation(unreal.Rotator(roll, 0.0, 0.0), False)
        right = ramp.get_actor_right_vector()
        if right.z < 0 and right.y > 0:
            break
    right = ramp.get_actor_right_vector()
    up = ramp.get_actor_up_vector()
    # The top face's line at local y = +(half length - UNDER) sits on the floor at y = FOOT_Y.
    seam_local_y = length_along / 2.0 - UNDER
    centre_y = FOOT_Y - seam_local_y * right.y - THICK / 2.0 * up.y
    centre_z = 0.0 - seam_local_y * right.z - THICK / 2.0 * up.z
    ramp.set_actor_location(unreal.Vector((X0 + X1) / 2.0, centre_y, centre_z), False, False)
    if ramp_grid:
        ramp.static_mesh_component.set_material(0, ramp_grid)
    unreal.log(f"[Wildreach] ramp roll {ramp.get_actor_rotation().roll:.1f} right {right} up {up} centre y {centre_y:.0f} z {centre_z:.0f}; "
               f"top {UP_SLOPE * math.sin(math.radians(SLOPE_DEG)):.0f} cm at y {FOOT_Y - UP_SLOPE * math.cos(math.radians(SLOPE_DEG)):.0f}")

    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (0, -45, 30), "Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.ExponentialHeightFog, label="HeightFog")

    # The survivor starts out of the way, behind where the cameras stand.
    start = spawn(unreal.PlayerStart, (-2500, 2500, 120), (0, 0, 0), "PlayerStart")

    if not les.save_current_level():
        raise RuntimeError("[Wildreach] save failed")
    unreal.log(f"[Wildreach] created {MAP_PATH}")
