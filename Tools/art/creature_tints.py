"""Tinted materials for pack creatures reused as Wildreach creatures. Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/art/creature_tints.py"

  /Game/Creatures/M_CreatureTint   master: pack textures, desaturated, tinted, brightened; glow from the illum map.
  MI_Gloomhowl   Barghest, pale grey and eyeless (no glow).
  MI_Scorchwing  Mountain Dragon, ember red with glowing markings.
  MI_Skaldrith   Mountain Dragon, ice blue with a cold glow.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MASTER = "/Game/Creatures/M_CreatureTint"
Q = "/Game/QuadrapedCreatures/"


def get_or_create(path, cls, factory):
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit("/", 1)
    return tools.create_asset(name, folder, cls, factory)


def build_master():
    mat = get_or_create(MASTER, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    n = [0]

    def expr(cls, **props):
        n[0] += 1
        e = mel.create_material_expression(mat, cls, -1200 + (n[0] % 6) * 200, (n[0] // 6) * 180)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def tex_param(name, default, sampler):
        return expr(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name,
                    texture=unreal.load_asset(default), sampler_type=sampler)

    def scalar(name, v):
        return expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)

    def vector(name, rgb):
        return expr(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1.0))

    def link(a, b, pin="", out=""):
        mel.connect_material_expressions(a, out, b, pin)

    base_t = tex_param("BaseColorTex", Q + "Barghest/Textures/BARGHEST_BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    norm_t = tex_param("NormalTex", Q + "Barghest/Textures/BARGHEST_NRM_2_Yinv", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    glow_t = tex_param("GlowTex", Q + "Barghest/Textures/BARGHEST_ILLUM", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    # Desaturate by hand (luminance, then lerp): the Desaturation node's pins don't connect from Python.
    lum = expr(unreal.MaterialExpressionDotProduct)
    link(base_t, lum, "A")
    link(expr(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1)), lum, "B")
    desat = expr(unreal.MaterialExpressionLinearInterpolate)
    link(base_t, desat, "A")
    link(lum, desat, "B")
    link(scalar("Desaturation", 1.0), desat, "Alpha")
    tinted = expr(unreal.MaterialExpressionMultiply)
    link(desat, tinted, "A")
    link(vector("Tint", (1, 1, 1)), tinted, "B")
    bright = expr(unreal.MaterialExpressionMultiply)
    link(tinted, bright, "A")
    link(scalar("Brightness", 1.0), bright, "B")
    glow = expr(unreal.MaterialExpressionMultiply)
    link(glow_t, glow, "A")
    link(vector("GlowColor", (1, 0.5, 0.1)), glow, "B")
    glow_k = expr(unreal.MaterialExpressionMultiply)
    link(glow, glow_k, "A")
    link(scalar("GlowStrength", 0.0), glow_k, "B")
    mel.connect_material_property(bright, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(norm_t, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(glow_k, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar("Roughness", 0.7), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def instance(path, textures, scalars, vectors):
    mi = get_or_create(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master)
    for k, v in textures.items():
        mel.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(v))
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1.0))
    eal.save_loaded_asset(mi)
    unreal.log(f"[Wildreach] tinted {path}")


master = build_master()
dragon = {"BaseColorTex": Q + "MountainDragon/Textures/T_MOUNTAIN_DRAGON_ALBEDO",
          "NormalTex": Q + "MountainDragon/Textures/T_MOUNTAIN_DRAGON_NRM",
          "GlowTex": Q + "MountainDragon/Textures/T_MOUNTAIN_DRAGON_ILLUM"}
instance("/Game/Creatures/Gloomhowl/MI_Gloomhowl",
         {"BaseColorTex": Q + "Barghest/Textures/BARGHEST_BaseColor", "NormalTex": Q + "Barghest/Textures/BARGHEST_NRM_2_Yinv",
          "GlowTex": Q + "Barghest/Textures/BARGHEST_ILLUM"},
         {"Desaturation": 1.0, "Brightness": 3.2, "GlowStrength": 0.0, "Roughness": 0.75}, {"Tint": (0.86, 0.88, 0.9)})
instance("/Game/Creatures/Scorchwing/MI_Scorchwing", dragon,
         {"Desaturation": 0.75, "Brightness": 1.1, "GlowStrength": 1.5, "Roughness": 0.6},
         {"Tint": (1.0, 0.32, 0.12), "GlowColor": (1.0, 0.4, 0.05)})
instance("/Game/Creatures/Skaldrith/MI_Skaldrith", dragon,
         {"Desaturation": 1.0, "Brightness": 1.9, "GlowStrength": 1.0, "Roughness": 0.45},
         {"Tint": (0.72, 0.86, 1.0), "GlowColor": (0.35, 0.7, 1.0)})
