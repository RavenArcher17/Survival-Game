"""The outfits' trim as a tint, not paint (round 11 look pass, Raven: "keep at it until they look perfect/correct").

M_WR_Armor and M_WR_Garment (Aaron's lab, not in the repo) draw a vertex whose colour R is 1 in the piece's trim colour
(its element's: arcane purple, wind cyan, fire orange...) times the texture, and the lab's meshes mark whole garments
so: Wardcloth's tights flat neon purple, Skysilk's cyan, Forgeplate's dark plates orange wherever the texture is bright
(a leopard print), the Survivor Wraps' strap a flat dark sticker. A new scalar, TrimAmount (default here), scales that
blend: the element colour stays each set's tint, over its own stuff. An instance may set its own. Safe to run again
(it finds its own changes and leaves them).

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import os
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
AMOUNT = float(os.environ.get("WR_TRIM", "0.4"))
log = []


def param(mat, near, name, value):
    for e in mel.get_material_expressions(mat):
        if isinstance(e, unreal.MaterialExpressionScalarParameter) and str(e.get_editor_property("parameter_name")) == name:
            e.set_editor_property("default_value", value)
            return e, False
    x = near.get_editor_property("material_expression_editor_x") - 300
    y = near.get_editor_property("material_expression_editor_y") + 200
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    return e, True


# ---- M_WR_Armor: the base colour node (HLSL) gets a TrimAmount input
armor = unreal.load_asset("/Game/Wildreach/Characters/Hero/Outfits/Armor/M_WR_Armor")
OLD = "c = lerp(c, TrimColor * saturate(Tex.rgb * TrimTexBoost), saturate(VC.r));"
NEW = "c = lerp(c, TrimColor * saturate(Tex.rgb * TrimTexBoost), saturate(VC.r) * TrimAmount);"
for e in mel.get_material_expressions(armor):
    if not isinstance(e, unreal.MaterialExpressionCustom):
        continue
    code = str(e.get_editor_property("code"))
    if OLD not in code and NEW not in code:
        continue
    names = [str(ci.get_editor_property("input_name")) for ci in e.get_editor_property("inputs")]
    p, made = param(armor, e, "TrimAmount", AMOUNT)
    if "TrimAmount" not in names:
        ins = list(e.get_editor_property("inputs"))
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", "TrimAmount")
        ins.append(ci)
        e.set_editor_property("inputs", ins)
    e.set_editor_property("code", code.replace(OLD, NEW))
    if not mel.connect_material_expressions(p, "", e, "TrimAmount"):
        raise RuntimeError("armor: TrimAmount not connected")
    log.append("M_WR_Armor %s: trim x TrimAmount %.2f (%s)" % (e.get_name(), AMOUNT, "new" if made else "updated"))
mel.recompile_material(armor)
eal.save_loaded_asset(armor)

# ---- M_WR_Garment: the base colour's lerp alpha (the vertex colour) x TrimAmount
garment = unreal.load_asset("/Game/Wildreach/Characters/Hero/Outfits/Common/M_WR_Garment")
lerp = mel.get_material_property_input_node(garment, unreal.MaterialProperty.MP_BASE_COLOR)
ins = mel.get_inputs_for_material_expression(garment, lerp)
alpha = ins[2] if len(ins) > 2 else None
if isinstance(alpha, unreal.MaterialExpressionVertexColor):
    p, _ = param(garment, alpha, "TrimAmount", AMOUNT)
    mul = mel.create_material_expression(garment, unreal.MaterialExpressionMultiply, alpha.get_editor_property("material_expression_editor_x") + 160, alpha.get_editor_property("material_expression_editor_y") + 120)
    mel.connect_material_expressions(alpha, "R", mul, "A")
    mel.connect_material_expressions(p, "", mul, "B")
    mel.connect_material_expressions(mul, "", lerp, "Alpha")
    log.append("M_WR_Garment: trim alpha = vertex R x TrimAmount %.2f (new)" % AMOUNT)
elif isinstance(alpha, unreal.MaterialExpressionMultiply):
    param(garment, alpha, "TrimAmount", AMOUNT)
    log.append("M_WR_Garment: TrimAmount default %.2f (updated)" % AMOUNT)
else:
    log.append("M_WR_Garment: base colour alpha is %s, left alone" % (alpha.get_class().get_name() if alpha else None))
mel.recompile_material(garment)
eal.save_loaded_asset(garment)
unreal.log("[Wildreach] outfit trim: " + "; ".join(log))
with open(unreal.Paths.project_saved_dir() + "fix_outfit_trim.txt", "w") as f:
    f.write("\n".join(log) + "\n")
