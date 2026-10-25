# Creates /Game/Creatures/M_Glow: an unlit emissive material for glowing creatures without art (the wisp).
# "Color" (same parameter name as the placeholder shapes' material, so status tints keep working) times "Glow".
# Run headless:  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/make_glow_material.py"
import unreal

PATH = "/Game/Creatures/M_Glow"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
if eal.does_asset_exist(PATH):
    print("[Wildreach] M_Glow already exists")
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = tools.create_asset("M_Glow", "/Game/Creatures", unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.6, 0.15, 1.0))
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -500, 200)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 8.0)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -250, 100)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(glow, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    print("[Wildreach] created M_Glow")
