# One-off: give the existing M_Rock the Nanite usage flag without rebuilding every terrain material (the cave boulders
# are Nanite meshes; without it cooked builds draw them in the default material). build_terrain_materials.py sets it too.
#   UnrealEditor-Cmd.exe Wildreach.uproject -run=pythonscript -script=Tools/terrain/fix_rock_nanite.py
import unreal

path = "/Game/Terrain/M_Rock"
mel = unreal.MaterialEditingLibrary
mat = unreal.load_asset(path)
if mat is None:
    raise RuntimeError(f"{path} not found")
mel.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_NANITE)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
unreal.log(f"FIXROCK {path} used_with_nanite={mat.get_editor_property('used_with_nanite')}")
