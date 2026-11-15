"""Materials for the Crystal caves (cooks/build-biome-crystal.md 2.0.5, 2.6: sound makes light). Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/terrain/build_crystal_materials.py"

Shares the graph helpers of Tools/make_build_materials.py (read from it, up to its first material).

  M_CrystalGlow  every crystal of the caves (the geode linings' amethyst points, the selenite beams, the Prism Hall's
                 lining, the dome bowls' points): vertex colour is the crystal's own colour (violet amethyst with paler
                 tips, white selenite, the doubling spar's water-clear), vertex alpha how much of it glows (1 at a point's
                 tip, 0.3 at its root, 0 for a beam's grey growth bands). Glassy (roughness 0.08, a fresnel rim).
                 It lights by sound: emissive = colour x GlowColor x GlowScale x (Rest + Lit + four rings), each ring a
                 world point (RingA..RingD .rgb, cm) with a radius (.a, cm) and a strength (RingS .r .g .b .a), so a
                 noise lights a sphere round it falling off with distance (WRCrystalWorld pushes the noises in and
                 fades their strengths over 6 s). Rest is the resting glow, Lit a struck keystone chime's whole-hall
                 light. Parameters: GlowColor, GlowScale, Rest, Lit, RingA..RingD, RingS.
"""
import os
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(_here, "..", "make_build_materials.py"), encoding="utf-8").read()
exec(_src[: _src.index("# ================================================================ M_BuildWood")])


def vparam(name, rgba):
    return expr(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgba))


def pin(src, out):
    """One channel of a node by its output pin (a vector parameter's or vertex colour's first pin is RGB only)."""
    e = expr(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    link(src, e, "", out)
    return e


def sparam(name, v):
    return expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)


# ================================================================ M_CrystalGlow
begin("/Game/Terrain/M_CrystalGlow")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
vc = expr(unreal.MaterialExpressionVertexColor)
rgb = mask(vc, "r", "g", "b")
alpha = pin(vc, "A")
wp = expr(unreal.MaterialExpressionWorldPosition)
strengths = vparam("RingS", (0.0, 0.0, 0.0, 0.0))
rings = None
for i, name in enumerate(("RingA", "RingB", "RingC", "RingD")):
    ring = vparam(name, (0.0, 0.0, -1.0e7, 100.0))
    dist = op(unreal.MaterialExpressionDistance, wp, mask(ring, "r", "g", "b"))
    radius = op(unreal.MaterialExpressionMax, pin(ring, "A"), None, const_b=1.0)
    fall = sat(sub(const(1.0), op(unreal.MaterialExpressionDivide, dist, radius)))
    # A ring is brightest at its middle and falls off smoothly (squared) to nothing at its radius.
    term = mul(mul(fall, fall), pin(strengths, "RGBA"[i]))
    rings = term if rings is None else add(rings, term)
rest = sparam("Rest", 0.15)
lit = sparam("Lit", 0.0)
level = add(add(rest, lit), rings)
glow_col = vparam("GlowColor", (0.62, 0.42, 1.0, 1.0))
scale = sparam("GlowScale", 6.0)
# How much of the crystal glows: the tips most (vertex alpha), never quite none (0.25) so a lining always shows its shape.
weight = add(mul(alpha, k=0.75), k=0.25)
emit = mul(mul(mul(mul(rgb, glow_col), scale), level), weight)
# The rim: crystal edges catch whatever light there is (a fresnel term scaled by the same level).
fres = expr(unreal.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.04)
rim = mul(mul(mul(fres, rgb), level), k=2.5)
mel.connect_material_property(add(emit, rim), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(mul(rgb, k=0.55), "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(const(0.08), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(const(0.9), "", unreal.MaterialProperty.MP_SPECULAR)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
