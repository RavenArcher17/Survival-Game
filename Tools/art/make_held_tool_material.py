"""M_HeldTool: the material of the modelled hand tools and the fishing float (Source/Wildreach/WRToolMesh.cpp).
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/art/make_held_tool_material.py". Safe to re-run.

The meshes carry their surface in vertex colour: RGB the colour (linear), A the roughness (0..1). The material adds:
  grain   the engine's tiling noise stretched along U (the meshes lay U along a part's length): streaks down a haft,
          long fibres in lashing, faint flaking on stone;
  grit    a finer noise across both, so flat stone faces don't read as plastic;
  wear    a lighter, smoother cast where the grain is high (hand polish on the haft, a sharpened edge).
Only engine content is used (no Fab pack), so a fresh clone builds it.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
PATH = "/Game/Wildreach/Materials/M_HeldTool"
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


def mask(src, *chans):
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


if eal.does_asset_exist(PATH):
    mat = unreal.load_asset(PATH)
    mel.delete_all_material_expressions(mat)
else:
    folder, name = PATH.rsplit("/", 1)
    mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

vc = expr(unreal.MaterialExpressionVertexColor)
rgb = mask(vc, "r", "g", "b")
rough = mask(vc, "a")
uv = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
u = mask(uv, "r")
v = mask(uv, "g")
# Grain: long along U (stretched 1:9), fine across V.
grain_uv = expr(unreal.MaterialExpressionAppendVector)
link(mul(u, k=0.35), grain_uv, "A")
link(mul(v, k=3.2), grain_uv, "B")
grain = noise(grain_uv)
# Grit: small and even.
grit = noise(mul(uv, k=4.5))
# Shade: 0.78 .. 1.12 by grain, a touch more by grit.
shade = add(lerp(const(0.78), const(1.12), grain), mul(add(grit, k=-0.5), k=0.12))
base = mul(rgb, shade)
# Wear: where the grain is high, a little smoother.
rough_out = expr(unreal.MaterialExpressionClamp, min_default=0.08, max_default=1.0)
link(add(rough, mul(add(grain, k=-0.5), k=-0.18)), rough_out, "Input")

mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(rough_out, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
