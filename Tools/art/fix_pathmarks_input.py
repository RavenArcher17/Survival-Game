"""Mends MF_PathMarks (the hero skin's path marks, in M_WR_Skin): every survivor's and townsperson's skin drew shiny
black (11-22).

The function had been built more than once and carried three "BaseColor" inputs (and two "UV"s). M_WR_Skin feeds its
skin colour into one of them (FunctionInput_6), but the code node that draws the marks over the skin (Custom_0, "Base")
read another (FunctionInput_0), which nothing fed: it defaulted to black, so the skin's base colour was black under every
mark. This wires Custom_0's Base to the fed input and deletes the stray inputs, so the function has one BaseColor, one
Emissive and one UV again. Safe to run again (it finds nothing to do).

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
FN = "/Game/Wildreach/Characters/Hero/Materials/MF_PathMarks"
MAT = "/Game/Wildreach/Characters/Hero/Materials/M_WR_Skin"
mf = unreal.load_asset(FN)
mat = unreal.load_asset(MAT)


def node(name):
    return unreal.find_object(None, f"{FN}.MF_PathMarks:{name}")


fed = node("MaterialExpressionFunctionInput_6")      # the BaseColor input M_WR_Skin feeds
code = node("MaterialExpressionCustom_0")             # the live base colour (Function output BaseColor)
stray = [n for n in (node("MaterialExpressionFunctionInput_0"), node("MaterialExpressionFunctionInput_3"),
                     node("MaterialExpressionFunctionInput_8")) if n]
log = []
if fed and code:
    ok = mel.connect_material_expressions(fed, "", code, "Base")
    log.append(f"Custom_0.Base <- FunctionInput_6: {ok}")
for n in stray:
    log.append(f"deleted {n.get_name()} ({n.get_editor_property('input_name')})")
    mel.delete_material_expression_in_function(mf, n)
if fed and code:
    mel.update_material_function(mf)
    eal.save_loaded_asset(mf)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
unreal.log("[Wildreach] path marks input: " + ("; ".join(log) if log else "nothing to do"))
