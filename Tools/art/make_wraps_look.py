"""The castaway's starter outfit (Survivor Wraps) and the held sword and shield, made to read as what they are
(round 11 look pass, Raven: "fix the cloths and stuff on the player"). Safe to run again (it remakes them).

  Wraps   Aaron's refitted pieces (lab, not in the repo) are all trim (vertex colour R = 1) on M_WR_Garment, so they drew
          in the shared slots' dark trim colour (TrimColor x the texture x TrimTexBoost 2.2): the strap a flat dark brown
          sticker, the kilt near black, the shin wraps brown tubes. One instance per part now, each in its own stuff and
          colour (TrimColor carries it; Tint matched in case a piece isn't trim):
            MI_Wraps_Strap   tanned leather, a little sheen (the man's chest strap, the woman's band)
            MI_Wraps_Kilt    weathered sailcloth, grey-brown (darker than skin, so it never reads as bare)
            MI_Wraps_Shins   pale linen strips, the rope's twist tiled small so the wrapping shows
            MI_Wraps_Rope    hemp (the belt, the bracers)
            MI_Wraps_Sole    dark rawhide (the sandal soles)
          Data/player_art.json looks.wraps "materials" points each slot at them.
  M_PropMetal   M_Prop's texture, with the metal in it found by colour: grey and bright (steel, iron) is metallic and
          smoother, warm (wood, leather) stays as it was; the sword's blade drew as matte near-white. MI_Sword_Metal and
          MI_Shield_Metal (each prop's own texture) on sword_game and shield_game.

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DIR = "/Game/Wildreach/Characters/Hero/Outfits/Common"
log = []

# ---------------------------------------------------------------- the wraps
# name: (copy of, (r, g, b) linear albedo wanted, roughness, tile, normal strength)
WRAPS = {
    "MI_Wraps_Strap": ("MI_Rawhide", (0.20, 0.11, 0.055), 0.55, 3.0, 1.3),
    "MI_Wraps_Kilt": ("MI_Linen_Dark", (0.15, 0.135, 0.11), 0.9, 3.5, 1.2),
    "MI_Wraps_Shins": ("MI_Rope", (0.36, 0.32, 0.25), 0.9, 7.0, 1.2),
    "MI_Wraps_Rope": ("MI_Rope", (0.30, 0.24, 0.15), 0.9, 4.0, 1.1),
    "MI_Wraps_Sole": ("MI_Rawhide", (0.09, 0.055, 0.03), 0.7, 2.0, 1.0),
}
for name, (src, albedo, rough, tile, nrm) in WRAPS.items():
    path = f"{DIR}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = eal.duplicate_asset(f"{DIR}/{src}", path)
    # trim draws TrimColor x texture x 2.2 (a texture ~0.45 bright): TrimColor ~ albedo; Tint (untrimmed) ~ albedo / 0.45
    trim = tuple(c * 1.0 for c in albedo)
    tint = tuple(min(2.0, c / 0.45) for c in albedo)
    mel.set_material_instance_vector_parameter_value(mi, "TrimColor", unreal.LinearColor(*trim, 1.0))
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "Tile", tile)
    mel.set_material_instance_scalar_parameter_value(mi, "NormalStrength", nrm)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    log.append(name)

# ---------------------------------------------------------------- M_PropMetal
PATH = "/Game/Props/M_PropMetal"
if eal.does_asset_exist(PATH):
    mat = unreal.load_asset(PATH)
    mel.delete_all_material_expressions(mat)
else:
    mat = tools.create_asset("M_PropMetal", "/Game/Props", unreal.Material, unreal.MaterialFactoryNew())
n = [0]


def ex(cls, **props):
    n[0] += 1
    e = mel.create_material_expression(mat, cls, -1400 + (n[0] % 6) * 220, (n[0] // 6) * 160)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin="", out=""):
    if not mel.connect_material_expressions(a, out, b, pin):
        raise RuntimeError(f"link {a.get_name()} -> {b.get_name()}:{pin}")


def binop(cls, a, b):
    e = ex(cls)
    link(a, e, "A")
    link(b, e, "B")
    return e


def k(v):
    return ex(unreal.MaterialExpressionConstant, r=v)


tex = ex(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="BaseColorTexture")
tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/DefaultTexture"))
rgb = ex(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
link(tex, rgb)
r_ = ex(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
link(tex, r_)
b_ = ex(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
link(tex, b_)
lum = ex(unreal.MaterialExpressionDotProduct)
link(rgb, lum, "A")
link(ex(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1.0)), lum, "B")
# warmth: red over blue (wood and leather are warm, steel is grey)
warm = ex(unreal.MaterialExpressionAbs)
link(binop(unreal.MaterialExpressionSubtract, r_, b_), warm)
grey = ex(unreal.MaterialExpressionSaturate)
link(binop(unreal.MaterialExpressionMultiply, binop(unreal.MaterialExpressionSubtract, k(0.09), warm), k(14.0)), grey)
bright = ex(unreal.MaterialExpressionSaturate)
link(binop(unreal.MaterialExpressionMultiply, binop(unreal.MaterialExpressionSubtract, lum, k(0.18)), k(5.0)), bright)
metal = binop(unreal.MaterialExpressionMultiply, grey, bright)
# metal's colour is its reflection's: a touch darker and cooler than the painted grey, so it mirrors rather than glows
steel = binop(unreal.MaterialExpressionMultiply, rgb, ex(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.62, 0.64, 0.68, 1.0)))
base = ex(unreal.MaterialExpressionLinearInterpolate)
link(rgb, base, "A")
link(steel, base, "B")
link(metal, base, "Alpha")
rough = ex(unreal.MaterialExpressionLinearInterpolate)
link(k(0.75), rough, "A")
link(k(0.3), rough, "B")
link(metal, rough, "Alpha")
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
log.append("M_PropMetal")

for prop, mi_name in (("Sword/sword_game", "MI_Sword_Metal"), ("Shield/shield_game", "MI_Shield_Metal")):
    sm = unreal.load_asset(f"/Game/Props/{prop}/StaticMeshes/{prop.split('/')[1]}")
    old = sm.static_materials[0].material_interface
    texture = mel.get_material_instance_texture_parameter_value(old, "BaseColorTexture") if isinstance(old, unreal.MaterialInstance) else None
    folder = f"/Game/Props/{prop}/Materials"
    path = f"{folder}/{mi_name}"
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = tools.create_asset(mi_name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", mat)
    if texture:
        mel.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", texture)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    sm.set_material(0, mi)
    eal.save_loaded_asset(sm, False)
    log.append(f"{prop} -> {mi_name} (texture {texture.get_name() if texture else None})")
unreal.log("[Wildreach] wraps look: " + "; ".join(log))
with open(unreal.Paths.project_saved_dir() + "make_wraps_look.txt", "w") as f:
    f.write("\n".join(log) + "\n")
