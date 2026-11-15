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
  M_ArcaneWard   hard light: shield films, Aegis, runes, the Twins' double, Lyrathis's beams and dome (additive, a
                 fresnel rim, vertex colour x Color x Glow; Face, Rim).
  M_MirrorWater  the Archive of Wards' still water: the real knight's mirrored pose drawn only where the visible surface
                 is the floor (scene depth turned back into a height); Tex, Tint, FloorZ, Strength.
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


# ================================================================ M_ArcaneWard
# Hard light (cooks/build-biome-crystal.md P2.4-P2.9): an Arcane shield's film, Aegis's dome, a rune's lines, the Twins'
# double's sheen, Lyrathis's beams and her Prism ward. Unlit, additive, two-sided: brightest at the silhouette (a fresnel
# rim: the edge of a shell of light) with a faint face so you see through it to what it guards; vertex colour tints it
# (the facets' and the stave's lines are painted brighter in the vertex alpha), and a slow shimmer crawls over it.
# Parameters: Color, Glow, Face (the face's share of the rim), Rim (the fresnel exponent).
def append2(a, b):
    return op(unreal.MaterialExpressionAppendVector, a, b)


begin("/Game/Terrain/M_ArcaneWard")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property("two_sided", True)
vc = expr(unreal.MaterialExpressionVertexColor)
rgb = mask(vc, "r", "g", "b")
alpha = pin(vc, "A")
colour = vparam("Color", (0.48, 0.25, 0.82, 1.0))
glow = sparam("Glow", 1.0)
face = sparam("Face", 0.18)
rim_exp = sparam("Rim", 2.5)
fres = expr(unreal.MaterialExpressionFresnel, base_reflect_fraction=0.0)
link(rim_exp, fres, "ExponentIn")
t = expr(unreal.MaterialExpressionTime)
wx, wy, wz = world_xyz()
shimmer = add(mul(noise(append2(mul(wx, k=1.0 / 90.0), add(mul(wz, k=1.0 / 70.0), mul(t, k=0.35)))), k=0.35), k=0.8)
lines = add(mul(alpha, k=2.0), k=0.4)
amount = mul(mul(mul(add(fres, face), lines), shimmer), glow)
mel.connect_material_property(mul(mul(rgb, colour), amount), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")


# ================================================================ M_MirrorWater
# The Archive of Wards' still water (cooks/biome-crystal.md 2.13.1; build-biome-crystal.md P2.2 2): only the real knight
# has a reflection. His pose is copied onto a mesh mirrored under the floor and drawn with this: translucent, unlit,
# drawn over the scene (no depth test) but only where the surface you see along that pixel is the floor itself (the
# scene's depth turned back into a height and compared with FloorZ), and only below it. So the reflection shows in the
# water and nowhere else: not through a pillar, not over the knight, not on a survivor standing in front of it.
# Parameters: Tex (the knight's colour map), Tint, FloorZ (cm, world), Strength (how strongly the water mirrors).
begin("/Game/Terrain/M_MirrorWater")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("disable_depth_test", True)
mat.set_editor_property("used_with_skeletal_mesh", True)
tex = expr(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="Tex")
tex.set_editor_property("texture", unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture"))
tint = vparam("Tint", (0.42, 0.36, 0.6, 1.0))
floor_z = sparam("FloorZ", 0.0)
strength = sparam("Strength", 0.55)
cam = expr(unreal.MaterialExpressionCameraPositionWS)
wp = expr(unreal.MaterialExpressionWorldPosition)
scene_d = expr(unreal.MaterialExpressionSceneDepth)
pixel_d = expr(unreal.MaterialExpressionPixelDepth)
cam_z = mask(cam, "b")
wp_z = mask(wp, "b")
ratio = op(unreal.MaterialExpressionDivide, scene_d, op(unreal.MaterialExpressionMax, pixel_d, None, const_b=1.0))
scene_z = add(cam_z, mul(sub(wp_z, cam_z), ratio))
on_floor = sat(sub(const(1.0), mul(unary(unreal.MaterialExpressionAbs, sub(scene_z, floor_z)), k=1.0 / 10.0)))
behind = sat(mul(sub(pixel_d, scene_d), k=0.2))
# Deeper in the water the image darkens (the black basalt under it), and it trembles faintly.
depth_fade = sat(sub(const(1.0), mul(sub(floor_z, wp_z), k=1.0 / 900.0)))
opacity = mul(mul(mul(on_floor, behind), depth_fade), strength)
mel.connect_material_property(mul(mask(tex, "r", "g", "b"), tint), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
