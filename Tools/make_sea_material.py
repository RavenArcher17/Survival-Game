"""The sea's material: /Game/Sea/MI_Ocean, an instance of the WaterMaterials pack's M_Ocean (moving water, shore foam,
deeper colour offshore). The game loads it by path (AWRSea), so /Game/Sea is cooked (DirectoriesToAlwaysCook) and the
pack's material and textures come along with it. Colours and other values are set at runtime from tuning.json "sea".
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/make_sea_material.py"   (safe to re-run)
"""
import unreal

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
PATH = "/Game/Sea/MI_Ocean"
PARENT = "/Game/WaterMaterials/Materials/M_Ocean"

parent = unreal.load_asset(PARENT)
if not parent:
    raise RuntimeError(f"{PARENT} missing: is the WaterMaterials pack in Content/?")
if eal.does_asset_exist(PATH):
    mi = unreal.load_asset(PATH)
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mi = tools.create_asset("MI_Ocean", "/Game/Sea", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(mi, parent)
eal.save_loaded_asset(mi)
unreal.log(f"[Wildreach] {PATH} -> {PARENT}")
