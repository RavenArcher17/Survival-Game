"""Creates /Game/Maps/L_Wildreach: a World Partition test map with a 4x4 km ground
plane, sky/sun/fog and a few player starts. Safe to re-run (skips if the map exists).

Run headless:  Tools/make_map.ps1
"""
import unreal

MAP_PATH = "/Game/Maps/L_Wildreach"
WORLD_SIZE_CM = 400000.0  # 4 km

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    unreal.log_warning(f"[Wildreach] {MAP_PATH} already exists, leaving it alone")
else:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if not les.new_level(MAP_PATH, True):
        raise RuntimeError(f"[Wildreach] new_level failed for {MAP_PATH}")

    def spawn(cls, loc=(0, 0, 0), rot=(0, 0, 0), label=None):
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
        if label:
            actor.set_actor_label(label)
        return actor

    # Ground: the engine plane is 100 cm, so scale it to 4 km.
    plane = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    ground = spawn(unreal.StaticMeshActor, label="Ground_4km")
    ground.static_mesh_component.set_static_mesh(plane)
    ground.set_actor_scale3d(unreal.Vector(WORLD_SIZE_CM / 100.0, WORLD_SIZE_CM / 100.0, 1.0))
    grid = unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")
    if grid:
        ground.static_mesh_component.set_material(0, grid)

    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (0, -40, 30), "Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)

    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.ExponentialHeightFog, label="HeightFog")
    spawn(unreal.VolumetricCloud, label="Clouds")

    # A few starts so several players don't spawn inside each other.
    for i, (x, y) in enumerate([(0, 0), (400, 0), (0, 400), (-400, 0), (0, -400)]):
        spawn(unreal.PlayerStart, (x, y, 150), label=f"PlayerStart_{i}")

    # Landmarks so movement is readable on a flat grid.
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    for i, (x, y, s) in enumerate([(3000, 0, 6), (-5000, 2000, 10), (0, -8000, 20), (12000, 9000, 30)]):
        marker = spawn(unreal.StaticMeshActor, (x, y, s * 50), label=f"Landmark_{i}")
        marker.static_mesh_component.set_static_mesh(cube)
        marker.set_actor_scale3d(unreal.Vector(s, s, s))

    if not les.save_current_level():
        raise RuntimeError("[Wildreach] save_current_level failed")
    unreal.log(f"[Wildreach] created {MAP_PATH}")
