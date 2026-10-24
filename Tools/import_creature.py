# Import a rigged creature FBX (from WildreachArt/rig_quadruped.py) into /Game/Creatures/<Name>.
# Run headless:  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/import_creature.py" with WR_FBX / WR_NAME set.
import os
import unreal

fbx = os.environ["WR_FBX"]
name = os.environ["WR_NAME"]
dest = f"/Game/Creatures/{name}"

task = unreal.AssetImportTask()
task.filename = fbx
task.destination_path = dest
task.automated = True
task.save = True
task.replace_existing = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

reg = unreal.AssetRegistryHelpers.get_asset_registry()
for a in reg.get_assets_by_path(dest, recursive=True):
    unreal.log(f"[Wildreach] imported {a.asset_class_path.asset_name} {a.package_name}")
unreal.EditorAssetLibrary.save_directory(dest)
