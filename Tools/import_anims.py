# Import animations only (no mesh) from an FBX onto an existing skeleton, e.g. new clips for a survivor without
# re-importing its mesh. The FBX comes from Tools/art/survivor_build.py (same rig, same rest pose). Give it a name no
# asset in the folder has (survivor_m_clips.fbx): an FBX named like the mesh re-imports the mesh instead.
# Uses Interchange (as the mesh import did): the classic FBX importer ignores the root "Armature" node's rotation and
# scale, which lays every clip flat on the ground. The pipeline goes in through an InterchangePipelineStackOverride:
# set straight on task.options it is ignored and the default stack imports a new mesh + skeleton instead.
# Each new clip is renamed <prefix><Clip> (e.g. survivor_mArmature_BowAim) to match Data/player_art.json's animPrefix.
# Run headless:  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/import_anims.py"
#   WR_ANIMS="C:\path\survivor_m_clips.fbx=/Game/Characters/Survivor/survivor_m_Skeleton=survivor_mArmature_;..."
import os
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
reg = unreal.AssetRegistryHelpers.get_asset_registry()
eal = unreal.EditorAssetLibrary


def setp(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as e:
        print(f"[Wildreach] (pipeline has no {name}: {e})")


for item in os.environ.get("WR_ANIMS", "").split(";"):
    if not item.strip():
        continue
    fbx, skel_path, prefix = (s.strip() for s in item.split("="))
    skeleton = unreal.load_asset(skel_path)
    if not skeleton:
        print(f"[Wildreach] no skeleton {skel_path}")
        continue
    dest = skel_path.rsplit("/", 1)[0]
    before = {str(a.asset_name) for a in reg.get_assets_by_path(dest)}
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
    setp(common, "import_only_animations", True)
    setp(common, "skeleton", skeleton)
    setp(pipeline.get_editor_property("mesh_pipeline"), "import_skeletal_meshes", False)
    setp(pipeline.get_editor_property("mesh_pipeline"), "import_static_meshes", False)
    setp(pipeline.get_editor_property("animation_pipeline"), "import_animations", True)
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = dest
    task.automated = True
    task.save = True
    task.replace_existing = True
    stack = unreal.InterchangePipelineStackOverride()  # a bare pipeline in task.options is ignored (default stack runs)
    stack.add_pipeline(pipeline)
    task.options = stack
    tools.import_asset_tasks([task])
    renamed = []
    for data in [a for a in reg.get_assets_by_path(dest) if str(a.asset_name) not in before]:
        name = str(data.asset_name)
        if str(data.asset_class_path.asset_name) != "AnimSequence" or name.endswith("_clips"):
            eal.delete_asset(f"{dest}/{name}")  # anything else the importer made (physics asset, bind-pose clip)
            continue
        clip = name.split("Armature_")[-1] if "Armature_" in name else name.split("_")[-1]
        target = f"{dest}/{prefix}{clip}"
        if eal.does_asset_exist(target):
            try:
                eal.delete_asset(target)
            except Exception as e:  # a clip left without a skeleton by a failed import still deletes, but raises
                print(f"[Wildreach] (deleting old {target}: {str(e).splitlines()[0]})")
        if eal.rename_asset(f"{dest}/{name}", target):
            renamed.append(f"{prefix}{clip}")
    eal.save_directory(dest)
    print(f"[Wildreach] anims into {dest}: {renamed}")
