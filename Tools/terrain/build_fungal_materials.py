"""Materials for the Fungal caverns (cooks/build-biome-fungal.md 2.0.9, 2.4: the caves light themselves). Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/terrain/build_fungal_materials.py"

Shares the graph helpers of Tools/make_build_materials.py (read from it, up to its first material).

  M_FungalGlow  every living light of the caverns: the glow-moss carpets, the ceiling glow, the caps' gills, the lure
                threads and the starfield, glow-cap rings, deepcaps, the Ancestor Cap's gills, Mycelorn's gills. Vertex
                colour is the surface's own colour (moss cyan, gill cyan, honey amber, thread white), vertex alpha how
                much of it glows (0: none, a cap's top; 1: a gill's edge). A soft organic surface (roughness 0.55, a
                faint sheen at grazing angles, like wet mushroom skin). emissive = rgb x alpha x GlowColor x GlowScale,
                plus a slow travelling shimmer (world position and time) so a carpet of moss is never flat light.
                GlowScale is the circadian curve and the bloom's / Sporefall's sync beat, set every second by
                UWRFungalWorld on the few shared instances (one per light colour). Parameters: GlowColor, GlowScale,
                Shimmer.
  M_FungalVeil  what light passes through: a Puffball's sac, the Rotmaw's false deepcap, the Sporemother's abdomen,
                Mycelorn's amber stipe, web sheets, the veil ring. Translucent, two-sided, lit: base = vertex rgb x Color,
                opacity = Opacity x (0.45 + fresnel) (thin at the face, denser at the silhouette), emissive = rgb x Color
                x Glow x (0.4 + vertex alpha). Parameters: Color, Opacity, Glow.
"""
import os
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(_here, "..", "make_build_materials.py"), encoding="utf-8").read()
exec(_src[: _src.index("# ================================================================ M_BuildWood")])


def vparam(name, rgba):
    return expr(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgba))


def pin(src, out):
    if isinstance(src, unreal.MaterialExpressionVertexColor):
        # Its main output is RGB only (UE 5.8): each channel from its own output.
        e = expr(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
        link(src, e, "", out)
        return e
    e = expr(unreal.MaterialExpressionComponentMask, r=out == "R", g=out == "G", b=out == "B", a=out == "A")
    link(src, e)
    return e


def sparam(name, v):
    return expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)


# ================================================================ M_FungalGlow
begin("/Game/Terrain/M_FungalGlow")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mat.set_editor_property("two_sided", True)
vc = expr(unreal.MaterialExpressionVertexColor)
rgb = mask(vc, "r", "g", "b")
alpha = pin(vc, "A")
glow_col = vparam("GlowColor", (0.22, 0.9, 0.76, 1.0))
scale = sparam("GlowScale", 4.0)
shimmer_amt = sparam("Shimmer", 0.25)
wp = expr(unreal.MaterialExpressionWorldPosition)
t = expr(unreal.MaterialExpressionTime)
# A slow shimmer: a sine over world position drifting with time (about 0.08 Hz), 0.75..1.25 at full Shimmer.
wave_in = add(add(mul(pin(wp, "R"), k=1.0 / 410.0), mul(pin(wp, "G"), k=1.0 / 530.0)), mul(t, k=0.5))
wave = unary(unreal.MaterialExpressionSine, wave_in)
shimmer = add(mul(wave, shimmer_amt), k=1.0)
emit = mul(mul(mul(mul(rgb, glow_col), scale), alpha), shimmer)
fres = expr(unreal.MaterialExpressionFresnel, exponent=4.0, base_reflect_fraction=0.04)
sheen = mul(mul(fres, rgb), k=0.35)
mel.connect_material_property(add(emit, sheen), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(mul(rgb, k=0.6), "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(const(0.55), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(0.35), "", unreal.MaterialProperty.MP_SPECULAR)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")


# ================================================================ M_FungalVeil
begin("/Game/Terrain/M_FungalVeil")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
vc = expr(unreal.MaterialExpressionVertexColor)
rgb = mask(vc, "r", "g", "b")
alpha = pin(vc, "A")
colour = vparam("Color", (1.0, 1.0, 1.0, 1.0))
opacity = sparam("Opacity", 0.55)
glow = sparam("Glow", 0.6)
fres = expr(unreal.MaterialExpressionFresnel, exponent=2.5, base_reflect_fraction=0.0)
op_amt = sat(mul(add(fres, k=0.45), opacity))
tint = mul(rgb, colour)
mel.connect_material_property(mul(tint, k=0.7), "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(mul(mul(tint, glow), add(alpha, k=0.4)), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(op_amt, "", unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(const(0.35), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
