"""M_TameMorph: the overlay a mutated creature wears over its own texture (cooks/build-taming.md 2.6; WRTameLook.cpp sets
it as the mesh's overlay material with its scalars). Mutations show on the body, one morph each:
  Pale      a pale, leucistic wash over the whole body (white leaning to its tint)
  Dark      a melanistic darkening of the whole body
  Bands     stripes across the body in its tint (bands from the mesh's own pre-skinned position, so they stay put
            on the hide as it moves)
  GlowLine  a glowing line along the top of the back in its tint (the top 12% of the mesh's own height)
Tint is the morph's colour. Translucent and lit, so a morph sits in the scene's light like the hide under it.
Only engine content is used (no Fab pack), so a fresh clone builds it.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/art/make_tame_morph_material.py". Safe to re-run.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
PATH = "/Game/Creatures/M_TameMorph"
mat = None
col = [0]


def expr(cls, **props):
    col[0] += 1
    e = mel.create_material_expression(mat, cls, -1800 + (col[0] % 8) * 230, (col[0] // 8) * 170)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin="", out=""):
    mel.connect_material_expressions(a, out, b, pin)


def op(cls, a, b=None, **consts):
    e = expr(cls, **consts)
    if a is not None:
        link(a, e, "A")
    if b is not None:
        link(b, e, "B")
    return e


def mul(a, b=None, k=None):
    return op(unreal.MaterialExpressionMultiply, a, b) if k is None else op(unreal.MaterialExpressionMultiply, a, const_b=k)


def add(a, b=None, k=None):
    return op(unreal.MaterialExpressionAdd, a, b) if k is None else op(unreal.MaterialExpressionAdd, a, const_b=k)


def sub(a, b):
    return op(unreal.MaterialExpressionSubtract, a, b)


def div(a, b):
    return op(unreal.MaterialExpressionDivide, a, b)


def mask(src, *chans, out=""):
    e = expr(unreal.MaterialExpressionComponentMask, r="r" in chans, g="g" in chans, b="b" in chans, a="a" in chans)
    link(src, e, "", out)
    return e


def const(v):
    return expr(unreal.MaterialExpressionConstant, r=v)


def const3(r, g, b):
    return expr(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1.0))


def lerp(a, b, alpha):
    e = expr(unreal.MaterialExpressionLinearInterpolate)
    link(a, e, "A")
    link(b, e, "B")
    link(alpha, e, "Alpha")
    return e


def clamp(x, lo, hi):
    e = expr(unreal.MaterialExpressionClamp, min_default=lo, max_default=hi)
    link(x, e, "Input")
    return e


def scalar(name, default):
    return expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=default)


def smooth(lo, hi, x):
    e = expr(unreal.MaterialExpressionSmoothStep, const_min=lo, const_max=hi)
    link(x, e, "Value")
    return e


if eal.does_asset_exist(PATH):
    mat = unreal.load_asset(PATH)
    mel.delete_all_material_expressions(mat)
else:
    folder, name = PATH.rsplit("/", 1)
    mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
mat.set_editor_property("used_with_skeletal_mesh", True)

tint = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Tint", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
tint3 = mask(tint, "r", "g", "b")
pale = scalar("Pale", 0.0)
dark = scalar("Dark", 0.0)
bands = scalar("Bands", 0.0)
glow = scalar("GlowLine", 0.0)
band_scale = scalar("BandScale", 0.045)

pos = expr(unreal.MaterialExpressionPreSkinnedPosition)
try:
    bounds = expr(unreal.MaterialExpressionPreSkinnedLocalBounds)
    bmin = mask(bounds, "r", "g", "b", out="Min")
    bmax = mask(bounds, "r", "g", "b", out="Max")
except Exception:
    bounds = expr(unreal.MaterialExpressionObjectLocalBounds)
    bmin = mask(bounds, "r", "g", "b", out="Min")
    bmax = mask(bounds, "r", "g", "b", out="Max")
height = div(sub(mask(pos, "b"), mask(bmin, "b")), add(sub(mask(bmax, "b"), mask(bmin, "b")), k=0.001))

# Bands: stripes across the hide from its own position (diagonal across X and Y, so any mesh facing reads striped).
stripe_in = mul(add(mask(pos, "r"), mask(pos, "g")), band_scale)
frac = expr(unreal.MaterialExpressionFrac)
link(stripe_in, frac)
stripe = mul(smooth(0.42, 0.5, frac), sub(const(1.0), smooth(0.86, 0.94, frac)))
band_w = mul(bands, stripe)

# The glow line: the top of the back.
line = smooth(0.86, 0.96, height)
glow_w = mul(glow, line)

pale_col = lerp(const3(0.92, 0.9, 0.86), tint3, const(0.25))
dark_col = const3(0.015, 0.014, 0.016)
band_col = mul(tint3, k=0.55)

w_pale = mul(pale, k=0.4)
w_dark = mul(dark, k=0.55)
w_band = mul(band_w, k=0.6)
w_glow = mul(glow_w, k=0.7)
total = add(add(add(w_pale, w_dark), add(w_band, w_glow)), k=0.0001)
colour = div(add(add(mul(pale_col, w_pale), mul(dark_col, w_dark)), add(mul(band_col, w_band), mul(tint3, w_glow))), total)
opacity = clamp(sub(total, const(0.0001)), 0.0, 0.85)
emissive = mul(mul(tint3, glow_w), k=6.0)

mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(const(0.7), "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
