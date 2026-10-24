# Import rigged creature FBXs (from Tools/art/rig_quadruped.py) into /Game/Creatures/<Name>.
# Run headless:  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/import_creature.py"
#   WR_IMPORTS="C:\path\a.fbx=Name;C:\path\b.fbx=Other"   (or WR_FBX + WR_NAME for one)
import os
import unreal

pairs = []
if os.environ.get("WR_IMPORTS"):
    for item in os.environ["WR_IMPORTS"].split(";"):
        if item.strip():
            path, name = item.rsplit("=", 1)
            pairs.append((path.strip(), name.strip()))
else:
    pairs.append((os.environ["WR_FBX"], os.environ["WR_NAME"]))

tools = unreal.AssetToolsHelpers.get_asset_tools()
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for fbx, name in pairs:
    dest = f"/Game/Creatures/{name}"
    # Re-importing over an existing folder only refreshes the mesh: the old texture and animations (with the old
    # skeleton's bone positions) stay. Make the caller move the old folder out of Content first.
    if reg.get_assets_by_path(dest, recursive=True):
        print(f"[Wildreach] SKIPPED {name}: {dest} already exists - move Content/Creatures/{name} out of the project, then import again")
        continue
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    unreal.EditorAssetLibrary.save_directory(dest)
    found = [str(a.asset_name) for a in reg.get_assets_by_path(dest, recursive=True)]
    print(f"[Wildreach] imported {name}: {', '.join(sorted(found))}")
