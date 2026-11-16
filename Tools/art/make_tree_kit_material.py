"""M_TreeKit, M_TreeKitLeaf, M_TreeKitGlow: the materials of the Ancient forest's code-built trees
(Source/Wildreach/WRTreeKit.cpp). Run once (headless, all six packs present):
    UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/art/make_tree_kit_material.py"
Safe to re-run. Only engine content is used, so a fresh clone builds them.

The tree meshes carry their surface in vertex colour: RGB the colour, stored sRGB-encoded (the static-mesh build
encodes it, which keeps dark bark from banding), A the roughness (0..1, linear). Every material decodes RGB with
pow 2.2. They are drawn instanced (HISM dressing, thousands of trees), so each is flagged for instanced static meshes:
a cooked build only has the shaders for the usages saved here.
  M_TreeKit      bark, roots, logs, fungus: grain stretched along U (the meshes lay U up a trunk / along a root, in
                 1.5 m units), so furrows run with the wood; a finer grit so smooth beech doesn't read as plastic.
  M_TreeKitLeaf  leaf sprays and fronds: a slow world-space mottle so neighbouring clusters differ, lower specular.
                 One-sided: the meshes build both faces of every leaf card.
  M_TreeKitGlow  the strips a caller lights: the heart-trees' amber sap in the bark cracks, foxfire streaks, the
                 Eldest's base cracks. Emissive = colour x Tint x Glow (scalar, default 2); MIDs set Glow for night
                 and the Bloom (WRTreeKit::GlowMaterial).
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
NOISE = "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq"
mat = None
col = [0]


def expr(cls, **props):
    col[0] += 1
    e = mel.create_material_expression(mat, cls, -1600 + (col[0] % 8) * 220, (col[0] // 8) * 170)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin="", out=""):
    # A pin name that doesn't exist fails silently and leaves the input empty (the material then fails to compile in
    # the cook and the game draws the default material). UE 5.8 names a single input "" (Clamp, OneMinus...), so
    # "Input" falls back to it; anything else that won't connect stops the script.
    if mel.connect_material_expressions(a, out, b, pin):
        return
    if pin == "Input" and mel.connect_material_expressions(a, out, b, ""):
        return
    raise RuntimeError(f"couldn't connect {a.get_name()}:{out!r} -> {b.get_name()}:{pin!r}")


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


def mask(src, *chans):
    # A Vertex Color node's main output is RGB only (UE 5.8): its alpha comes from its own "A" output.
    if isinstance(src, unreal.MaterialExpressionVertexColor) and "a" in chans:
        if len(chans) != 1:
            raise RuntimeError("vertex colour alpha goes alone (its main output has no alpha)")
        e = expr(unreal.MaterialExpressionComponentMask, r=True)
        link(src, e, "", "A")
        return e
    e = expr(unreal.MaterialExpressionComponentMask, r="r" in chans, g="g" in chans, b="b" in chans, a="a" in chans)
    link(src, e)
    return e


def const(v):
    return expr(unreal.MaterialExpressionConstant, r=v)


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


def noise(uv):
    tex = unreal.load_asset(NOISE)
    cs = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    if cs in (unreal.TextureCompressionSettings.TC_GRAYSCALE, unreal.TextureCompressionSettings.TC_ALPHA):
        st = unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    elif cs == unreal.TextureCompressionSettings.TC_MASKS:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    else:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    e = expr(unreal.MaterialExpressionTextureSample, texture=tex, sampler_type=st,
             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    link(uv, e, "UVs")
    return mask(e, "r")


def begin(path):
    global mat
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        folder, name = path.rsplit("/", 1)
        mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    u = unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
    mel.set_base_material_usage(mat, u) if hasattr(mel, "set_base_material_usage") else mel.set_material_usage(mat, u)
    mat.set_editor_property("two_sided", False)
    col[0] = 0


def finish(base, rough, emissive=None, specular=None):
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive is not None:
        mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if specular is not None:
        mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")


def vertex_surface():
    """(linear rgb, roughness) from the vertex colour: RGB decoded from sRGB."""
    vc = expr(unreal.MaterialExpressionVertexColor)
    rgb = expr(unreal.MaterialExpressionPower, const_exponent=2.2)  # pins: Base, Exponent (not A/B)
    link(mask(vc, "r", "g", "b"), rgb, "Base")
    return rgb, mask(vc, "a")


# ---------------------------------------------------------------- bark
begin("/Game/Wildreach/Materials/M_TreeKit")
rgb, rough = vertex_surface()
uv = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
u, v = mask(uv, "r"), mask(uv, "g")
grain_uv = expr(unreal.MaterialExpressionAppendVector)
link(mul(u, k=0.4), grain_uv, "A")
link(mul(v, k=3.0), grain_uv, "B")
grain = noise(grain_uv)
grit = noise(mul(uv, k=5.0))
shade = add(lerp(const(0.72), const(1.14), grain), mul(add(grit, k=-0.5), k=0.14))
finish(mul(rgb, shade), clamp(add(rough, mul(add(grain, k=-0.5), k=-0.12)), 0.1, 1.0), specular=const(0.35))

# ---------------------------------------------------------------- leaves
begin("/Game/Wildreach/Materials/M_TreeKitLeaf")
rgb, rough = vertex_surface()
wp = expr(unreal.MaterialExpressionWorldPosition)
mottle = noise(mul(mask(wp, "r", "g"), k=1.0 / 900.0))
fine = noise(mul(mask(wp, "r", "b"), k=1.0 / 120.0))
shade = add(lerp(const(0.82), const(1.12), mottle), mul(add(fine, k=-0.5), k=0.1))
finish(mul(rgb, shade), clamp(rough, 0.2, 1.0), specular=const(0.3))

# ---------------------------------------------------------------- glow strips
begin("/Game/Wildreach/Materials/M_TreeKitGlow")
rgb, rough = vertex_surface()
tint = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
glow = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=2.0)
finish(mul(rgb, k=0.4), clamp(rough, 0.2, 1.0), emissive=mul(mul(rgb, mask(tint, "r", "g", "b")), glow))
