"""Building materials: world-aligned planks for wood pieces, staggered blocks for stone. Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/make_build_materials.py"

  M_BuildWood   20 cm planks with offset end joints, each plank its own shade, grain from the engine's tiling noise.
  M_BuildStone  45 x 90 cm blocks in running bond with dark mortar, each block its own shade, mottled.

Both project from the world, picked by the face's normal: walls get horizontal courses along their length, floors and
roofs run along X. So pieces line up with their neighbours and need no UVs (the gable is a procedural mesh).
Only engine content is used (no Fab pack), so a fresh clone builds them.

Per-block mode (the polish pass's detail blocks, WRBuildLook): an instanced block carries custom data 0 = its shade
(0..1), 1 = the world axis its grain runs along (0 X, 1 Y, 2 Z), 2 = a brightness factor (dark backing, iron). Then the
painted seams / mortar lines go (the blocks' real gaps show instead) and the whole face is one plank or stone in that
shade. Without custom data (plain components) the default -1 shade keeps the world-aligned pattern above.
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
NOISE = "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq"
mat = None
col = [0]  # layout cursor so the graph is readable if someone opens it


def begin(path):
    """Load (or create) a material and clear its graph."""
    global mat
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        folder, name = path.rsplit("/", 1)
        mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    col[0] = 0


def finish(base, rough):
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Detail blocks are instanced static meshes: without the usage flag a cooked game draws the default material.
    mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")


def expr(cls, **props):
    col[0] += 1
    e = mel.create_material_expression(mat, cls, -2400 + (col[0] % 12) * 200, (col[0] // 12) * 160)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin="", out=""):
    mel.connect_material_expressions(a, out, b, pin)


def op(cls, a, b=None, **consts):
    """Binary node; pass None for an input to use its const_a / const_b instead."""
    e = expr(cls, **consts)
    if a is not None:
        link(a, e, "A")
    if b is not None:
        link(b, e, "B")
    return e


def add(a, b=None, k=None):
    return op(unreal.MaterialExpressionAdd, a, b) if k is None else op(unreal.MaterialExpressionAdd, a, const_b=k)


def sub(a, b):
    return op(unreal.MaterialExpressionSubtract, a, b)


def mul(a, b=None, k=None):
    return op(unreal.MaterialExpressionMultiply, a, b) if k is None else op(unreal.MaterialExpressionMultiply, a, const_b=k)


def unary(cls, x, **props):
    e = expr(cls, **props)
    link(x, e)
    return e


def mask(src, *chans):
    e = expr(unreal.MaterialExpressionComponentMask, r="r" in chans, g="g" in chans, b="b" in chans, a="a" in chans)
    link(src, e)
    return e


def const3(rgb):
    return expr(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def const(v):
    return expr(unreal.MaterialExpressionConstant, r=v)


def lerp(a, b, alpha):
    e = expr(unreal.MaterialExpressionLinearInterpolate)
    link(a, e, "A")
    link(b, e, "B")
    link(alpha, e, "Alpha")
    return e


def sat(x):
    return unary(unreal.MaterialExpressionSaturate, x)


def frac(x):
    return unary(unreal.MaterialExpressionFrac, x)


def floor(x):
    return unary(unreal.MaterialExpressionFloor, x)


def hash1(x):
    """0..1 noise per whole number: frac(sin(x) * 43758.5)."""
    return frac(mul(unary(unreal.MaterialExpressionSine, x, period=6.2831853), k=43758.5453))


def edge(f, width):
    """0 within `width` of a cell border (f = position across the cell, 0..1), 1 inside: seams and mortar."""
    near = op(unreal.MaterialExpressionMin, f, unary(unreal.MaterialExpressionOneMinus, f))
    return sat(mul(near, k=1.0 / width))


def noise(uv):
    tex = unreal.load_asset(NOISE)
    # The sampler type has to match the texture's compression, or the material fails to compile.
    cs = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    unreal.log(f"[Wildreach] noise texture: {cs}, srgb {srgb}")
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


def courses():
    """(u, v) in cm: along a face and across its courses. Walls: u along the wall, v up. Floors / roofs: u = X, v = Y."""
    wp = expr(unreal.MaterialExpressionWorldPosition)
    n = expr(unreal.MaterialExpressionVertexNormalWS)
    ax = unary(unreal.MaterialExpressionAbs, mask(n, "r"))
    ay = unary(unreal.MaterialExpressionAbs, mask(n, "g"))
    az = unary(unreal.MaterialExpressionAbs, mask(n, "b"))
    flat = sat(mul(add(az, k=-0.65), k=20.0))   # 1 on floors and roofs (a 26 degree roof still counts)
    facing_x = sat(mul(sub(ax, ay), k=20.0))    # 1 on a wall facing along X: it runs along Y
    x, y, z = mask(wp, "r"), mask(wp, "g"), mask(wp, "b")
    u = lerp(lerp(x, y, facing_x), x, flat)
    v = lerp(z, y, flat)
    return u, v


def custom(index, default):
    return expr(unreal.MaterialExpressionPerInstanceCustomData, data_index=index, const_default_value=default)


def block_mode():
    """(shade, axis, brightness, use): use is 1 for a detail block (its shade >= 0), 0 for a plain component."""
    shade, axis, dim = custom(0, -1.0), custom(1, 0.0), custom(2, 1.0)
    use = sat(mul(add(shade, k=0.5), k=1000.0))
    return shade, axis, dim, use


def world_xyz():
    wp = expr(unreal.MaterialExpressionWorldPosition)
    return mask(wp, "r"), mask(wp, "g"), mask(wp, "b")


# ================================================================ M_BuildWood
begin("/Game/Building/M_BuildWood")
u, v = courses()
pv = mul(v, k=1.0 / 20.0)                        # 20 cm planks
row = floor(pv)
row_hash = hash1(mul(row, k=12.9898))
pu = mul(add(u, mul(row_hash, k=240.0)), k=1.0 / 240.0)  # 2.4 m boards, end joints staggered per row
plank = hash1(add(mul(row, k=7.13), mul(floor(pu), k=3.71)))
seams = mul(edge(frac(pv), 0.03), edge(frac(pu), 0.004))
grain = noise(op(unreal.MaterialExpressionAppendVector, add(mul(u, k=1.0 / 150.0), plank), mul(v, k=1.0 / 5.0)))
tone = sat(add(add(mul(plank, k=0.45), mul(grain, k=0.35)), k=0.2))
# per-block mode: the block's own shade, grain along its axis, no painted seams
shade, axis, dim, use = block_mode()
wx, wy, wz = world_xyz()
a1 = sat(axis)
a2 = sat(add(axis, k=-1.0))
ui = lerp(lerp(wx, wy, a1), wz, a2)
vi = sub(add(add(wx, wy), wz), ui)
grain_i = noise(op(unreal.MaterialExpressionAppendVector, add(mul(ui, k=1.0 / 150.0), mul(shade, k=7.0)), mul(vi, k=1.0 / 5.0)))
tone_i = sat(add(add(mul(shade, k=0.45), mul(grain_i, k=0.35)), k=0.2))
tone = lerp(tone, tone_i, use)
seams = lerp(seams, const(1.0), use)
wood = lerp(const3((0.075, 0.042, 0.022)), const3((0.28, 0.17, 0.085)), tone)
base = mul(mul(wood, lerp(const(0.35), const(1.0), seams)), dim)
finish(base, lerp(const(0.95), const(0.75), seams))

# ================================================================ M_BuildStone
begin("/Game/Building/M_BuildStone")
u, v = courses()
pv = mul(v, k=1.0 / 45.0)                        # 45 cm courses
row = floor(pv)
offset = mul(frac(mul(row, k=0.5)), k=90.0)      # every other course shifted half a block
pu = mul(add(u, offset), k=1.0 / 90.0)           # 90 cm blocks
block = hash1(add(mul(row, k=12.9898), mul(floor(pu), k=78.233)))
mortar = mul(edge(frac(pv), 0.05), edge(frac(pu), 0.025))
mottle = noise(op(unreal.MaterialExpressionAppendVector, mul(u, k=1.0 / 70.0), mul(v, k=1.0 / 70.0)))
tone = sat(add(add(mul(block, k=0.45), mul(mottle, k=0.4)), k=0.1))
# per-block mode: the block's own shade, no painted mortar
shade, axis, dim, use = block_mode()
tone = lerp(tone, sat(add(add(mul(shade, k=0.45), mul(mottle, k=0.4)), k=0.1)), use)
mortar = lerp(mortar, const(1.0), use)
stone = lerp(const3((0.09, 0.085, 0.08)), const3((0.30, 0.28, 0.25)), tone)
base = mul(lerp(const3((0.045, 0.04, 0.035)), stone, mortar), dim)
finish(base, const(0.9))
