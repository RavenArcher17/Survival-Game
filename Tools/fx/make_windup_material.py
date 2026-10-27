"""The creature attack wind-up glow (AWRCreature's telegraph in WRCreature.cpp). Safe to re-run.
  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/fx/make_windup_material.py"

  /Game/FX/M_WindupGlow  drawn over a creature's own materials (its overlay material) while it winds up an attack:
                         unlit, translucent, a soft wash with a bright rim. Shape = lerp(Wash, 1, fresnel);
                         emissive = Color * Glow * shape, opacity = saturate(Opacity * shape). The creature code ramps
                         Glow and Opacity through the wind-up. Flagged for skeletal meshes (the creatures' art; a cooked
                         build won't draw a material on a mesh type it wasn't flagged for). Partly undoes auto-exposure,
                         like M_Telegraph, so it reads at noon and at night.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
path = "/Game/FX/M_WindupGlow"

if eal.does_asset_exist(path):
    mat = unreal.load_asset(path)
    mel.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_WindupGlow", "/Game/FX", unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("used_with_skeletal_mesh", True)

n = [0]


def expr(cls, **props):
    n[0] += 1
    e = mel.create_material_expression(mat, cls, -1200 + (n[0] % 5) * 220, (n[0] // 5) * 180)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin="", out=""):
    if not mel.connect_material_expressions(a, out, b, pin):
        raise RuntimeError(f"could not link {a.get_name()}.{out} -> {b.get_name()}.{pin}")


def mul(a, b):
    e = expr(unreal.MaterialExpressionMultiply)
    link(a, e, "A")
    link(b, e, "B")
    return e


color = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.1, 0.02, 1.0))
glow = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=3.0)
opacity = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Opacity", default_value=0.6)
wash = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Wash", default_value=0.3)

# The rim: bright where the surface turns away from the camera, so the outline lights up against any ground.
fresnel = expr(unreal.MaterialExpressionFresnel, exponent=2.2, base_reflect_fraction=0.0)
shape = expr(unreal.MaterialExpressionLinearInterpolate, const_b=1.0)
link(wash, shape, "A")
link(fresnel, shape, "Alpha")

emissive = mul(mul(color, glow), shape)
try:
    inv = expr(unreal.MaterialExpressionEyeAdaptationInverse)
    lerp = expr(unreal.MaterialExpressionLinearInterpolate, const_a=1.0, const_alpha=0.6)
    link(inv, lerp, "B")
    emissive = mul(emissive, lerp)
except Exception as ex:
    unreal.log(f"[Wildreach] M_WindupGlow: no EyeAdaptationInverse ({ex})")
alpha = expr(unreal.MaterialExpressionSaturate)
link(mul(opacity, shape), alpha)
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] WINDUP built {path} ({n[0]} expressions)")
