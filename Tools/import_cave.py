# Import Blender-made cave meshes (Tools/art/cave_gen.py) into /Game/Props/Caves/<Name> with walkable collision.
# Run headless:  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/import_cave.py"
#   WR_IMPORTS="C:\path\cave_a.fbx=CaveA;C:\path\cave_b.fbx=CaveB"
# Collision is the mesh itself ("use complex as simple"), so the tunnel and chamber walls block exactly where they are
# drawn. No Nanite: complex collision wants the real triangles, and there are only a handful of caves.
# An existing folder is skipped: move it out of Content first to replace a cave.
import os
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for item in os.environ["WR_IMPORTS"].split(";"):
    if not item.strip():
        continue
    path, name = (s.strip() for s in item.rsplit("=", 1))
    dest = f"/Game/Props/Caves/{name}"
    if reg.get_assets_by_path(dest, recursive=True):
        print(f"[Wildreach] SKIPPED {name}: {dest} already exists")
        continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    for a in reg.get_assets_by_path(dest, recursive=True):
        mesh = a.get_asset()
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
        b = mesh.get_bounds()
        print(f"[Wildreach] imported {name}: {a.package_name}.{a.asset_name}, {mesh.get_num_triangles(0)} tris, "
              f"size {b.box_extent.x * 2:.0f} x {b.box_extent.y * 2:.0f} x {b.box_extent.z * 2:.0f} cm, centre {b.origin}")
    unreal.EditorAssetLibrary.save_directory(dest)
