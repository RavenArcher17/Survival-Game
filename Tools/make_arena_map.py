"""Creates /Game/Maps/L_CombatArena: a small flat arena for combat testing (M1).
Training dummies in the middle, a Cinderfang pack to the west and a Gloomhowl pack to the
east, player starts tagged by biome. Re-run with -force to rebuild: Tools/make_map.ps1 arena
"""
import os
import unreal

MAP_PATH = "/Game/Maps/L_CombatArena"
# make_map.ps1 -Force sets this (the python commandlet doesn't forward script arguments).
FORCE = os.environ.get("WR_FORCE") == "1"

eal = unreal.EditorAssetLibrary
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
        actor = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(*rot))
        if label:
            actor.set_actor_label(label)
        return actor

    plane = unreal.load_asset("/Engine/BasicShapes/Plane.Plane")
    cube = unreal.load_asset("/Engine/BasicShapes/Cube.Cube")
    grid = unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial")

    ground = spawn(unreal.StaticMeshActor, label="Arena_Floor")
    ground.static_mesh_component.set_static_mesh(plane)
    ground.set_actor_scale3d(unreal.Vector(120.0, 120.0, 1.0))  # 120 m square
    if grid:
        ground.static_mesh_component.set_material(0, grid)

    # Low walls so nothing wanders off the edge.
    for i, (x, y, sx, sy) in enumerate([(6000, 0, 1, 120), (-6000, 0, 1, 120), (0, 6000, 120, 1), (0, -6000, 120, 1)]):
        wall = spawn(unreal.StaticMeshActor, (x, y, 100), label=f"Arena_Wall_{i}")
        wall.static_mesh_component.set_static_mesh(cube)
        wall.set_actor_scale3d(unreal.Vector(sx, sy, 2.0))

    sun = spawn(unreal.DirectionalLight, (0, 0, 1000), (0, -45, 30), "Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.SkyAtmosphere, label="SkyAtmosphere")
    sky = spawn(unreal.SkyLight, (0, 0, 500), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    spawn(unreal.ExponentialHeightFog, label="HeightFog")

    # Spawns: fire mains start west-ish, frost mains east-ish, both near the dummies.
    for i, (x, y, tag) in enumerate([(-900, -300, "volcanic"), (-900, 300, "volcanic"), (900, -300, "tundra"), (900, 300, "tundra")]):
        start = spawn(unreal.PlayerStart, (x, y, 120), (0, 0 if x < 0 else 180, 0), f"PlayerStart_{tag}_{i}")
        start.set_editor_property("player_start_tag", tag)

    dummy_class = unreal.load_class(None, "/Script/Wildreach.WRTrainingDummy")
    for i, y in enumerate([-500, 0, 500]):
        spawn(dummy_class, (0, y, 100), (0, 0, 0), f"Dummy_{i}")

    spawner_class = unreal.load_class(None, "/Script/Wildreach.WRCreatureSpawner")
    for label, creature, x in [("Spawner_Cinderfang", "cinderfang", -3500), ("Spawner_Gloomhowl", "gloomhowl", 3500)]:
        sp = spawn(spawner_class, (x, 0, 50), (0, 0, 0), label)
        sp.set_editor_property("creature_id", creature)
        sp.set_editor_property("count", 3)
        sp.set_editor_property("radius", 400.0)
        sp.set_editor_property("respawn_seconds", 15.0)

    # Boss altars (the real ones sit in the boss arenas; here they're out in the open for testing).
    altar_class = unreal.load_class(None, "/Script/Wildreach.WRBossAltar")
    for label, path, x in [("Altar_Fire", "fire", -2000), ("Altar_Frost", "frost", 2000)]:
        altar = spawn(altar_class, (x, 1500, 50), (0, 0, 0), label)
        altar.set_editor_property("path_id", path)

    if not les.save_current_level():
        raise RuntimeError("[Wildreach] save failed")
    unreal.log(f"[Wildreach] created {MAP_PATH}")
