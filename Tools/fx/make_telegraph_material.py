"""The ground telegraph material for boss attacks and hazards (AWRGroundMarker in WRBoss.cpp). Safe to re-run.
  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/fx/make_telegraph_material.py"

  /Game/FX/M_Telegraph  unlit, translucent, gently pulsing. Emissive = Color * Glow * pulse, opacity = Opacity * pulse.
                        The markers were the engine's lit shape material tinted orange, which strong sun blew out to a flat
                        white or yellow patch. Partly undoes auto-exposure, like M_FxAdd, so it reads at noon and at night.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
path = "/Game/FX/M_Telegraph"

if eal.does_asset_exist(path):
    mat = unreal.load_asset(path)
    mel.delete_all_material_expressions(mat)
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_Telegraph", "/Game/FX", unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

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


color = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.3, 0.02, 1.0))
glow = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=1.6)
opacity = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Opacity", default_value=0.42)

# pulse = 0.78 + 0.22 * sin(time * 2pi / 0.9 s): it breathes, so it reads as a warning rather than a floor tile.
time = expr(unreal.MaterialExpressionTime)
sine = expr(unreal.MaterialExpressionSine, period=0.9)
link(time, sine)
amp = expr(unreal.MaterialExpressionMultiply, const_b=0.22)
link(sine, amp, "A")
pulse = expr(unreal.MaterialExpressionAdd, const_b=0.78)
link(amp, pulse, "A")

emissive = mul(mul(color, glow), pulse)
try:
    inv = expr(unreal.MaterialExpressionEyeAdaptationInverse)
    lerp = expr(unreal.MaterialExpressionLinearInterpolate, const_a=1.0, const_alpha=0.6)
    link(inv, lerp, "B")
    emissive = mul(emissive, lerp)
except Exception as ex:
    unreal.log(f"[Wildreach] M_Telegraph: no EyeAdaptationInverse ({ex})")
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(mul(opacity, pulse), "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] TELEGRAPH built {path} ({n[0]} expressions)")
