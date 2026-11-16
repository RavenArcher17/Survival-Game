"""Greenhouse glass (cooks/cookpass-survival.md 3.7, LATER-COOKS build-farming): M_WindowGlass, a clear translucent glass
for the glass walls and roofs (WRFarmLook's glazing sheet; tuning farm.glassMaterial). Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/terrain/build_glass_material.py" -unattended -nullrhi

  Old crown glass as a homestead would have it: a faint green-grey cast (iron in the sand), nearly clear looking straight
  through (opacity 0.12), more of a mirror toward grazing angles (Fresnel up to 0.6 opacity, the sky and sun caught on the
  pane), smooth (roughness 0.05) with a strong specular glint, lit per pixel on its surface so the sun's highlight sits on
  the pane, two-sided, casting no shadow of its own (the sheet turns that off in code). A slow ripple in the cast from world
  position stands for the waves in hand-blown panes.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
PATH = "/Game/Wildreach/Materials/M_WindowGlass"

if eal.does_asset_exist(PATH):
    mat = unreal.load_asset(PATH)
    mel.delete_all_material_expressions(mat)
else:
    mat = asset_tools.create_asset("M_WindowGlass", "/Game/Wildreach/Materials", unreal.Material, unreal.MaterialFactoryNew())

mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
try:
    mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
except Exception as e:  # older names
    unreal.log_warning(f"[Wildreach] glass: lighting mode not set ({e})")


def node(cls, x, y):
    return mel.create_material_expression(mat, cls, x, y)


# The cast: pale green-grey, rippling a little across the pane (two slow sine waves of world position).
tint = node(unreal.MaterialExpressionConstant3Vector, -900, -200)
tint.set_editor_property("constant", unreal.LinearColor(0.70, 0.80, 0.76, 1.0))
clear = node(unreal.MaterialExpressionConstant3Vector, -900, -50)
clear.set_editor_property("constant", unreal.LinearColor(0.86, 0.92, 0.90, 1.0))
wp = node(unreal.MaterialExpressionWorldPosition, -1300, 100)
mask = node(unreal.MaterialExpressionComponentMask, -1150, 100)
mask.set_editor_property("r", True)
mask.set_editor_property("g", True)
mask.set_editor_property("b", True)
mel.connect_material_expressions(wp, "", mask, "")
dot = node(unreal.MaterialExpressionDotProduct, -1000, 120)
dirv = node(unreal.MaterialExpressionConstant3Vector, -1150, 220)
dirv.set_editor_property("constant", unreal.LinearColor(0.031, 0.017, 0.043, 0.0))
mel.connect_material_expressions(mask, "", dot, "A")
mel.connect_material_expressions(dirv, "", dot, "B")
sine = node(unreal.MaterialExpressionSine, -850, 120)
mel.connect_material_expressions(dot, "", sine, "")
half = node(unreal.MaterialExpressionConstantBiasScale, -700, 120)
half.set_editor_property("bias", 1.0)
half.set_editor_property("scale", 0.5)
mel.connect_material_expressions(sine, "", half, "")
cast = node(unreal.MaterialExpressionLinearInterpolate, -500, -100)
mel.connect_material_expressions(tint, "", cast, "A")
mel.connect_material_expressions(clear, "", cast, "B")
mel.connect_material_expressions(half, "", cast, "Alpha")

# Opacity: clear face-on, a mirror at grazing angles.
fres = node(unreal.MaterialExpressionFresnel, -700, 300)
fres.set_editor_property("exponent", 4.0)
fres.set_editor_property("base_reflect_fraction", 0.04)
lo = node(unreal.MaterialExpressionConstant, -700, 420)
lo.set_editor_property("r", 0.12)
hi = node(unreal.MaterialExpressionConstant, -700, 500)
hi.set_editor_property("r", 0.6)
op = node(unreal.MaterialExpressionLinearInterpolate, -450, 380)
mel.connect_material_expressions(lo, "", op, "A")
mel.connect_material_expressions(hi, "", op, "B")
mel.connect_material_expressions(fres, "", op, "Alpha")

rough = node(unreal.MaterialExpressionConstant, -450, 560)
rough.set_editor_property("r", 0.05)
spec = node(unreal.MaterialExpressionConstant, -450, 640)
spec.set_editor_property("r", 0.8)

mel.connect_material_property(cast, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log("[Wildreach] built M_WindowGlass")
