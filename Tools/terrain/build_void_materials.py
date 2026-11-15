"""Materials for runtime lands and the void (cooks/build-biome-void.md 2.2, 2.6). Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/terrain/build_void_materials.py"

Shares the graph helpers of Tools/make_build_materials.py (read from it, up to its first material).

  M_LandGround  a runtime land's ground (WRLand chunks, the Great Rift's walls): vertex colour is the ground's colour
                (set per point from its sub-zone, rift rings, slope), vertex alpha its gloss (1 = voidglass, 0 = ash).
                Detail from the MW auto-material's rock / dirt textures projected from the world on three axes and
                taken as brightness only, so the land keeps the palette law (grey land, black glass) and still reads
                as real ground up close.
  M_VoidTear    the inside of a rift: unlit black-violet (#2B1840) with star points that sit in screen space (so the
                tear looks deeper than the thing it's cut in), a violet rim at grazing angles (#7B4FD6), a slow breath.
                Parameters: Glow (overall), Stars (0..1), Rim (colour).
"""
import os
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(_here, "..", "make_build_materials.py"), encoding="utf-8").read()
exec(_src[: _src.index("# ================================================================ M_BuildWood")])

ROCK = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Rock_col"
DIRT = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Dirt_col"


def tex_sample(path, uv):
    tex = unreal.load_asset(path)
    cs = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    elif cs in (unreal.TextureCompressionSettings.TC_GRAYSCALE, unreal.TextureCompressionSettings.TC_ALPHA):
        st = unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    elif cs == unreal.TextureCompressionSettings.TC_MASKS:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    else:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    e = expr(unreal.MaterialExpressionTextureSample, texture=tex, sampler_type=st,
             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    link(uv, e, "UVs")
    return e


def luminance(rgb_node):
    return expr_dot(rgb_node, (0.299, 0.587, 0.114))


def expr_dot(a, w):
    d = expr(unreal.MaterialExpressionDotProduct)
    link(a, d, "A")
    link(const3(w), d, "B")
    return d


def append2(a, b):
    return op(unreal.MaterialExpressionAppendVector, a, b)


# ================================================================ M_LandGround
begin("/Game/Terrain/M_LandGround")
vc = expr(unreal.MaterialExpressionVertexColor)
rgb = mask(vc, "r", "g", "b")
gloss = mask(vc, "a")
wx, wy, wz = world_xyz()
n = expr(unreal.MaterialExpressionVertexNormalWS)
nx = unary(unreal.MaterialExpressionAbs, mask(n, "r"))
ny = unary(unreal.MaterialExpressionAbs, mask(n, "g"))
nz = unary(unreal.MaterialExpressionAbs, mask(n, "b"))
# three projections, weighted by the normal (sharpened), so slopes and the rift's walls don't smear
wsum = add(add(nx, ny), nz)
bx = op(unreal.MaterialExpressionDivide, mul(mul(nx, nx), nx), wsum)
by = op(unreal.MaterialExpressionDivide, mul(mul(ny, ny), ny), wsum)
bz = op(unreal.MaterialExpressionDivide, mul(mul(nz, nz), nz), wsum)
btot = add(add(bx, by), bz)
scale = 1.0 / 420.0
uv_top = mul(append2(wx, wy), k=scale)
uv_x = mul(append2(wy, wz), k=scale)
uv_y = mul(append2(wx, wz), k=scale)
rock_top = luminance(tex_sample(DIRT, uv_top))
rock_x = luminance(tex_sample(ROCK, uv_x))
rock_y = luminance(tex_sample(ROCK, uv_y))
detail = op(unreal.MaterialExpressionDivide, add(add(mul(rock_top, bz), mul(rock_x, bx)), mul(rock_y, by)), btot)
# a second, larger scale breaks the tiling
uv_far = mul(append2(wx, wy), k=1.0 / 3100.0)
far = luminance(tex_sample(DIRT, uv_far))
bright = add(mul(detail, k=1.25), mul(far, k=0.5))
base = mul(rgb, add(bright, k=0.25))
# glass reads smooth: less detail on it
base = lerp(base, mul(rgb, k=1.0), mul(gloss, k=0.7))
rough = sat(add(unary(unreal.MaterialExpressionOneMinus, gloss), mul(add(detail, k=-0.45), k=0.18)))
spec = lerp(const(0.35), const(0.65), gloss)
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")

# ================================================================ M_VoidTear
begin("/Game/Terrain/M_VoidTear")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided", True)
glow = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=1.0)
stars_on = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Stars", default_value=1.0)
rim_col = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Rim", default_value=unreal.LinearColor(0.198, 0.080, 0.672, 1.0))
core = const3((0.0242, 0.0091, 0.0513))  # #2B1840 linear
sp = expr(unreal.MaterialExpressionScreenPosition)
suv = mask(sp, "r", "g")
t = expr(unreal.MaterialExpressionTime)
drift = mul(t, k=0.004)
s1 = noise(add(mul(suv, k=9.0), drift))
s2 = noise(add(mul(suv, k=23.0), mul(drift, k=-1.7)))
# star points: only the brightest specks of two noise layers survive
pts = add(sat(mul(add(s1, k=-0.86), k=9.0)), sat(mul(add(s2, k=-0.9), k=12.0)))
star = mul(mul(const3((0.80, 0.78, 1.0)), pts), stars_on)
fres = expr(unreal.MaterialExpressionFresnel, exponent=2.2, base_reflect_fraction=0.0)
breath = add(mul(unary(unreal.MaterialExpressionSine, t, period=7.0), k=0.18), k=0.82)
rim = mul(mul(rim_col, fres), mul(breath, k=3.2))
emissive = mul(add(add(core, star), rim), glow)
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")

# ================================================================ M_VoidLight
# Gathered or bent light: the Great Rift's column, its lensing rings, the Black Beacon's cone, a tear's halo. Additive
# and translucent, brightest where you look straight through it and gone at its silhouette (an inverted fresnel), with
# a soft depth fade where it meets the ground. Parameters: Color, Glow, Edge (how fast it thins toward the silhouette).
begin("/Game/Terrain/M_VoidLight")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property("two_sided", True)
colour = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(0.48, 0.31, 0.84, 1.0))
glow = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=1.0)
edge = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Edge", default_value=2.0)
fres = expr(unreal.MaterialExpressionFresnel, base_reflect_fraction=0.0)
link(edge, fres, "ExponentIn")
facing = unary(unreal.MaterialExpressionOneMinus, fres)
t = expr(unreal.MaterialExpressionTime)
wx, wy, wz = world_xyz()
# slow bands drifting up through it (the light "swims")
bands = add(mul(noise(append2(mul(wx, k=1.0 / 2600.0), add(mul(wz, k=1.0 / 1800.0), mul(t, k=-0.03)))), k=0.6), k=0.55)
fade = expr(unreal.MaterialExpressionDepthFade, fade_distance_default=400.0)
amount = mul(mul(mul(facing, bands), glow), fade)
mel.connect_material_property(mul(colour, amount), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
