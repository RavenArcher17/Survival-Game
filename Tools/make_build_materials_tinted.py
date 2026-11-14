"""Building materials for metal, the 12 biome sets and build mode (build-building, cooks/build-building.md). Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/make_build_materials_tinted.py"

Shares the graph helpers of make_build_materials.py (read from it, up to its first material) without rebuilding the wood
and stone materials.

  M_BuildTinted   every tinted detail block: colour, metalness, roughness, glow and surface pattern per instance.
  M_BuildOverlay  the see-through stability colour build mode lays over pieces.
"""
import os
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(_here, "make_build_materials.py"), encoding="utf-8").read()
exec(_src[: _src.index("# ================================================================ M_BuildWood")])

# ================================================================ M_BuildTinted (build-building: metal and the 12 biome sets)
# One material for every tinted block (WRBuildLook EWRLookMat::Tinted): per-instance custom data
#   0 shade (0..1)  1 grain axis (0 X, 1 Y, 2 Z)  2 brightness  3..5 colour (linear RGB)  6 metallic  7 roughness
#   8 glow (emissive x colour)  9 pattern: 0 mottled stone, 1 wood grain, 2 brushed metal, 3 smooth (glass, glaze)
# So 13 building tiers and every set's furniture share one material and one draw per shape.
begin("/Game/Building/M_BuildTinted")
shade, axis, dim = custom(0, 0.5), custom(1, 0.0), custom(2, 1.0)
rgb = op(unreal.MaterialExpressionAppendVector, op(unreal.MaterialExpressionAppendVector, custom(3, 0.5), custom(4, 0.5)), custom(5, 0.5))
metal, rough, glow, pattern = custom(6, 0.0), custom(7, 0.85), custom(8, 0.0), custom(9, 0.0)
wx, wy, wz = world_xyz()
a1 = sat(axis)
a2 = sat(add(axis, k=-1.0))
ui = lerp(lerp(wx, wy, a1), wz, a2)
vi = sub(add(add(wx, wy), wz), ui)
seed = mul(shade, k=7.0)
grain_n = noise(op(unreal.MaterialExpressionAppendVector, add(mul(ui, k=1.0 / 150.0), seed), mul(vi, k=1.0 / 5.0)))
brush_n = noise(op(unreal.MaterialExpressionAppendVector, add(mul(ui, k=1.0 / 400.0), seed), mul(vi, k=1.0 / 0.8)))
mottle_n = noise(op(unreal.MaterialExpressionAppendVector, add(mul(ui, k=1.0 / 70.0), seed), mul(vi, k=1.0 / 70.0)))
# weights of the four patterns (pattern is a whole number)
w_mottle = sat(unary(unreal.MaterialExpressionOneMinus, pattern))
w_grain = sat(unary(unreal.MaterialExpressionOneMinus, unary(unreal.MaterialExpressionAbs, add(pattern, k=-1.0))))
w_brush = sat(unary(unreal.MaterialExpressionOneMinus, unary(unreal.MaterialExpressionAbs, add(pattern, k=-2.0))))
n = add(add(mul(mottle_n, w_mottle), mul(grain_n, w_grain)), mul(mul(brush_n, w_brush), k=0.6))
w_smooth = sat(add(pattern, k=-2.0))
amount = lerp(const(0.45), const(0.08), w_smooth)
tone = sat(add(add(mul(shade, k=0.35), mul(n, amount)), k=0.3))
base = mul(mul(rgb, lerp(const(0.72), const(1.18), tone)), dim)
r_out = sat(add(rough, mul(add(n, k=-0.5), k=0.12)))
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(r_out, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(mul(mul(rgb, glow), k=4.0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")

# ================================================================ M_BuildOverlay (build mode's stability colours)
# A see-through, unlit wash laid over a piece (UMeshComponent overlay material): "Color" by how much span it has left,
# brighter at grazing angles so edges read.
begin("/Game/Building/M_BuildOverlay")
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
colour = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(0.2, 0.9, 0.3, 1.0))
fres = expr(unreal.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.0)
mel.connect_material_property(mul(colour, k=1.6), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(sat(add(mul(fres, k=0.35), k=0.28)), "", unreal.MaterialProperty.MP_OPACITY)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
