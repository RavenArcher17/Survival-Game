"""Building materials: world-aligned planks for wood pieces, staggered blocks for stone. Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/make_build_materials.py"

  M_BuildWood   20 cm planks with offset end joints, each plank its own shade, grain from the engine's tiling noise.
  M_BuildStone  45 x 90 cm blocks in running bond with dark mortar, each block its own shade, mottled.

Both project from the world, picked by the face's normal: walls get horizontal courses along their length, floors and
roofs run along X. So pieces line up with their neighbours and need no UVs (the gable is a procedural mesh).

11-22 (Raven: "everything as good as the caves"): real surfaces on the same courses, with their relief as a world-space
normal (tangent_space_normal off):
  stone  SimpleRocks' rock (TX_Rock_05, a pack the project requires), each block its own patch of it;
  wood   the scanned boards of WoodMaterialPack2 (not required): texture parameters WoodColor / WoodNormal / WoodRough
         and Scan (0 = the old procedural planks) are set at run time when the pack is there (FWRBuildLook::Material),
         so the material itself references only engine textures and a machine without the pack keeps today's look.

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
    # A pin name that doesn't exist fails silently and leaves the input empty (the material then fails to compile in
    # the cook and the game draws the default material). UE 5.8 names a single input "" (Clamp, OneMinus...), so
    # "Input" falls back to it; anything else that won't connect stops the script.
    if mel.connect_material_expressions(a, out, b, pin):
        return
    if pin == "Input" and mel.connect_material_expressions(a, out, b, ""):
        return
    raise RuntimeError(f"couldn't connect {a.get_name()}:{out!r} -> {b.get_name()}:{pin!r}")


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
    # A Vertex Color node's main output is RGB only (UE 5.8): its alpha comes from its own "A" output.
    if isinstance(src, unreal.MaterialExpressionVertexColor) and "a" in chans:
        if len(chans) != 1:
            raise RuntimeError("vertex colour alpha goes alone (its main output has no alpha)")
        e = expr(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
        link(src, e, "", "A")
        return e
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
    x, y, z = mask(wp, "r"), mask(wp, "g"), mask(wp, "b")
    # along a wall: its level direction (cross(up, n)), so a wall at any yaw keeps square courses
    u = lerp(dot(wp, level_dir(n)), x, flat)
    v = lerp(z, y, flat)
    return u, v


def dot(a, b):
    d = expr(unreal.MaterialExpressionDotProduct)
    link(a, d, "A")
    link(b, d, "B")
    return d


def cross(a, b):
    c = expr(unreal.MaterialExpressionCrossProduct)
    link(a, c, "A")
    link(b, c, "B")
    return c


def level_dir(n):
    """The level direction along a face: cross(up, n), normalised (X where the face is level: n straight up/down)."""
    return normalized(add(cross(const3((0.0, 0.0, 1.0)), n), const3((0.0001, 0.0, 0.0))))


def to_world(v):
    """A vector in the block's own (instance) space turned and scaled into the world's. (Instance space: "local" on an
    instanced component is the component's, which turned none of the blocks and scaled them all 1.)"""
    e = expr(unreal.MaterialExpressionTransform,
             transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE,
             transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(v, e, "Input")
    return e


def vlen(v):
    return unary(unreal.MaterialExpressionSquareRoot, dot(v, v))


def round_info(axis_raw):
    """A detail block's axis (custom data 1: 0/1/2, plus 3 for a cylinder or cone, WRBuildLook Spawn) and its roundness:
    (axis 0..2, how much this pixel is on the block's round side 0..1, the arc round its axis in cm here)."""
    cyl = sat(mul(add(axis_raw, k=-2.5), k=1000.0))
    axis = sub(axis_raw, mul(cyl, k=3.0))
    lp = expr(unreal.MaterialExpressionTransformPosition,
              transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
              transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_INSTANCE)
    link(expr(unreal.MaterialExpressionWorldPosition), lp, "Input")
    at = expr(unreal.MaterialExpressionArctangent2)
    link(mask(lp, "g"), at, "Y")
    link(mask(lp, "r"), at, "X")
    r_here = mul(vlen(append(mask(lp, "r"), mask(lp, "g"))), vlen(to_world(const3((1.0, 0.0, 0.0)))))
    n = expr(unreal.MaterialExpressionVertexNormalWS)
    up = normalized(to_world(const3((0.0, 0.0, 1.0))))
    side = sat(mul(sub(const(0.95), unary(unreal.MaterialExpressionAbs, dot(n, up))), k=8.0))
    return axis, mul(cyl, side), mul(at, r_here)


def block_frame(axis, rnd=None):
    """A detail block's face frame: (along, across, u, v), u and v in cm on the face along and across. along is the
    block's grain: custom data axis 0/1/2 names the world axis its long side lies nearest (WRBuildLook Add), so it is the
    block's own side (turned with it) nearest that axis, laid into the face; on an end face (the grain along the normal)
    the face's level direction (X on a level face) stands in."""
    n = expr(unreal.MaterialExpressionVertexNormalWS)
    X, Y, Z = const3((1.0, 0.0, 0.0)), const3((0.0, 1.0, 0.0)), const3((0.0, 0.0, 1.0))
    W = lerp(lerp(X, Y, sat(axis)), Z, sat(add(axis, k=-1.0)))
    lx, ly, lz = normalized(to_world(X)), normalized(to_world(Y)), normalized(to_world(Z))
    dx, dy, dz = [unary(unreal.MaterialExpressionAbs, dot(l, W)) for l in (lx, ly, lz)]
    t = lerp(lerp(lx, ly, sat(mul(sub(dy, dx), k=1000.0))), lz, sat(mul(sub(dz, op(unreal.MaterialExpressionMax, dx, dy)), k=1000.0)))
    c = cross(n, t)
    end = sat(mul(add(vlen(c), k=-0.35), k=-5.0))          # 1 where the axis runs (nearly) along the normal
    az = unary(unreal.MaterialExpressionAbs, mask(n, "b"))
    fb = lerp(level_dir(n), X, sat(mul(add(az, k=-0.65), k=20.0)))
    along = normalized(lerp(cross(normalized(add(c, const3((0.0001, 0.0, 0.0)))), n), fb, end))
    across = cross(n, along)
    wp = expr(unreal.MaterialExpressionWorldPosition)
    v = dot(wp, across)
    if rnd is not None:
        v = lerp(v, rnd[1], rnd[0])   # round blocks: across is the arc round them
    return along, across, dot(wp, along), v


def block_big():
    """1 on a block whose middle side is over ~1.6 m (a wall, a floor: it draws its own joints), 0 on a small one."""
    X, Y, Z = const3((1.0, 0.0, 0.0)), const3((0.0, 1.0, 0.0)), const3((0.0, 0.0, 1.0))
    a, b, c = vlen(to_world(X)), vlen(to_world(Y)), vlen(to_world(Z))   # the cube is 100 units a side: x100 = cm
    mn = op(unreal.MaterialExpressionMin, a, b)
    mid = op(unreal.MaterialExpressionMax, mn, op(unreal.MaterialExpressionMin, op(unreal.MaterialExpressionMax, a, b), c))
    return sat(mul(add(mul(mid, k=100.0), k=-140.0), k=1.0 / 40.0))


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


SSM = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS
GRID = "/Engine/EngineMaterials/T_Default_Material_Grid_M"   # a linear-colour default (the wood scans are linear)
FLAT = "/Engine/EngineMaterials/DefaultNormal"


def sampler_for(tex):
    cs = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        return unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    if cs == unreal.TextureCompressionSettings.TC_MASKS:
        return unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    if cs in (unreal.TextureCompressionSettings.TC_GRAYSCALE, unreal.TextureCompressionSettings.TC_ALPHA):
        return unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    return unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR


def tex(path, uv):
    t = unreal.load_asset(path)
    e = expr(unreal.MaterialExpressionTextureSample, texture=t, sampler_type=sampler_for(t), sampler_source=SSM)
    link(uv, e, "UVs")
    return e


def tex_param(name, default_path, uv):
    """A texture set at run time (a pack's, when the machine has it); the default fixes the sampler type."""
    t = unreal.load_asset(default_path)
    e = expr(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, texture=t, sampler_type=sampler_for(t), sampler_source=SSM)
    link(uv, e, "UVs")
    return e


def scalar_p(name, v):
    return expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)


def vector_p(name, rgb):
    return expr(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def append(a, b):
    return op(unreal.MaterialExpressionAppendVector, a, b)


def normalized(v):
    e = expr(unreal.MaterialExpressionNormalize)
    link(v, e, "VectorInput")
    return e


def power(x, p):
    e = expr(unreal.MaterialExpressionPower, const_exponent=p)
    link(x, e, "Base")
    return e


def course_axes():
    """The world directions of courses()'s u and v: walls u along the wall, v up; floors and roofs u = X, v = Y."""
    n = expr(unreal.MaterialExpressionVertexNormalWS)
    ax = unary(unreal.MaterialExpressionAbs, mask(n, "r"))
    ay = unary(unreal.MaterialExpressionAbs, mask(n, "g"))
    az = unary(unreal.MaterialExpressionAbs, mask(n, "b"))
    flat = sat(mul(add(az, k=-0.65), k=20.0))
    X, Y, Z = const3((1.0, 0.0, 0.0)), const3((0.0, 1.0, 0.0)), const3((0.0, 0.0, 1.0))
    return n, lerp(level_dir(n), X, flat), lerp(Z, Y, flat)


def block_axes(n, axis):
    """A detail block's grain direction (custom data axis: 0 X, 1 Y, 2 Z) and the direction across it on the face."""
    X, Y, Z = const3((1.0, 0.0, 0.0)), const3((0.0, 1.0, 0.0)), const3((0.0, 0.0, 1.0))
    t = lerp(lerp(X, Y, sat(axis)), Z, sat(add(axis, k=-1.0)))
    c = expr(unreal.MaterialExpressionCrossProduct)
    link(n, c, "A")
    link(t, c, "B")
    return t, normalized(add(c, const3((0.0001, 0.0001, 0.0001))))


def bumped(n, across, along, tn, strength):
    """The face's normal bent by a normal map sampled with U across and V along (world-space, as the caves')."""
    return normalized(add(n, mul(add(mul(across, mask(tn, "r")), mul(along, mask(tn, "g"))), k=strength)))


def finish_n(base, rough, normal, specular=None):
    mat.set_editor_property("tangent_space_normal", False)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    if specular is not None:
        mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    finish(base, rough)


# ================================================================ M_BuildWood
begin("/Game/Building/M_BuildWood")
u, v = courses()
# A detail block's boards follow its own frame (grain along its axis); a plain piece's the face's courses. A big block
# (a floor, a board wall) draws its boards' joints; a small one is one board.
shade, axis_raw, dim, use = block_mode()
axis, rw, arc = round_info(axis_raw)
b_al, b_ac, bu, bv = block_frame(axis, (rw, arc))
solo = mul(use, sub(const(1.0), block_big()))
cu, cv = lerp(u, bu, use), lerp(v, bv, use)
pv = mul(cv, k=1.0 / 20.0)                       # 20 cm planks
row = floor(pv)
row_hash = hash1(mul(row, k=12.9898))
pu = mul(add(cu, mul(row_hash, k=240.0)), k=1.0 / 240.0)  # 2.4 m boards, end joints staggered per row
plank = hash1(add(mul(row, k=7.13), mul(floor(pu), k=3.71)))
seams = mul(edge(frac(pv), 0.03), edge(frac(pu), 0.004))
pid = lerp(plank, shade, solo)
grain = noise(op(unreal.MaterialExpressionAppendVector, add(mul(cu, k=1.0 / 150.0), mul(pid, k=7.0)), mul(cv, k=1.0 / 5.0)))
tone = sat(add(add(mul(pid, k=0.45), mul(grain, k=0.35)), k=0.2))
seams = lerp(seams, const(1.0), solo)
wood = lerp(const3((0.075, 0.042, 0.022)), const3((0.28, 0.17, 0.085)), tone)
base = mul(mul(wood, lerp(const(0.35), const(1.0), seams)), dim)
rough = lerp(const(0.95), const(0.75), seams)
# The scanned boards: each board (or detail block) its own patch of the scan, its grain (the scan's V) along the board,
# its own shade; the seams darker. 1.4 m to a tile.
n, T, B = course_axes()
along_d, across_d = lerp(T, b_al, use), lerp(B, b_ac, use)
uv = append(add(mul(cv, k=1.0 / 140.0), mul(pid, k=5.37)), add(mul(cu, k=1.0 / 140.0), mul(pid, k=2.71)))
scan_c = power(mask(tex_param("WoodColor", GRID, uv), "r", "g", "b"), 2.2)
scan_n = tex_param("WoodNormal", FLAT, uv)
scan_r = mask(tex_param("WoodRough", GRID, uv), "r")
scan = scalar_p("Scan", 0.0)
scan_base = mul(mul(mul(scan_c, vector_p("WoodTint", (0.62, 0.5, 0.4))), lerp(const(0.72), const(1.15), pid)),
                mul(lerp(const(0.3), const(1.0), seams), dim))
base = lerp(base, scan_base, scan)
rough = lerp(rough, lerp(const(0.6), const(0.95), scan_r), scan)
normal = lerp(n, bumped(n, across_d, along_d, scan_n, 0.9), scan)
finish_n(base, rough, normalized(normal))

# ================================================================ M_BuildStone
begin("/Game/Building/M_BuildStone")
u, v = courses()
shade, axis_raw, dim, use = block_mode()
axis, rw, arc = round_info(axis_raw)
rw = mul(rw, use)
u = lerp(u, arc, rw)                             # round towers: the courses run round them
pv = mul(v, k=1.0 / 36.0)                        # 36 cm courses (11-22: at 45 x 90 they read oversized)
row = floor(pv)
row_len = add(mul(hash1(mul(row, k=3.17)), k=26.0), k=62.0)   # each course its own block length, 62-88 cm
offset = mul(hash1(mul(row, k=12.9898)), k=90.0)  # and its own offset
pu = op(unreal.MaterialExpressionDivide, add(u, offset), row_len)
block = hash1(add(mul(row, k=12.9898), mul(floor(pu), k=78.233)))
mortar = mul(edge(frac(pv), 0.06), edge(frac(pu), 0.03))
# the stone's edge rounded into the joint: shaded across the outer fifth of each face
bevel = mul(edge(frac(pv), 0.2), edge(frac(pu), 0.1))
mottle = noise(op(unreal.MaterialExpressionAppendVector, mul(u, k=1.0 / 70.0), mul(v, k=1.0 / 70.0)))
tone = sat(add(add(mul(block, k=0.3), mul(mottle, k=0.35)), k=0.22))
# per-block mode: a small block is one stone, its own shade, no mortar; a big one (a wall) keeps the courses
solo = mul(use, sub(const(1.0), block_big()))
tone = lerp(tone, sat(add(add(mul(shade, k=0.45), mul(mottle, k=0.4)), k=0.1)), solo)
mortar = lerp(mortar, const(1.0), solo)
# Each block its own patch of SimpleRocks' rock (1.6 m to a tile), its own shade; the mortar dark between.
n, T, B = course_axes()
b_al, b_ac, bu, bv = block_frame(axis, (rw, arc))
bid = lerp(block, shade, solo)
su, sv = lerp(u, bu, solo), lerp(v, bv, solo)
uv = append(add(mul(su, k=1.0 / 105.0), mul(bid, k=3.31)), add(mul(sv, k=1.0 / 105.0), mul(bid, k=1.93)))
rock = mask(tex("/Game/SimpleRocks/Master_Materials/Textures/TX_Rock_05_D", uv), "r", "g", "b")
# (11-22, quick5: its dark blotches and pale veins read as marble tiles) its contrast halved, and each stone a blend
# toward the pack's sandstone (TX_Rock_08, at a finer grain), so the courses mix grey and warmer stones
rock = lerp(rock, const3((0.2, 0.2, 0.21)), const(0.45))
uv2 = append(add(mul(su, k=1.0 / 70.0), mul(bid, k=2.13)), add(mul(sv, k=1.0 / 70.0), mul(bid, k=4.31)))
sand = mul(mask(tex("/Game/SimpleRocks/Master_Materials/Textures/TX_Rock_08_D", uv2), "r", "g", "b"), k=0.7)
rock = lerp(rock, sand, mul(hash1(mul(bid, k=91.7)), k=0.55))   # (fully one or the other read as a checkerboard)
rock_n = tex("/Game/SimpleRocks/Master_Materials/Textures/TX_Rock_05_N", uv)
stone = mul(mul(rock, vector_p("StoneTint", (0.95, 0.9, 0.84))), lerp(const(0.72), const(1.12), tone))
# (round 9) a detail block's own tint, custom data 3-5 (WRBuildLook Spawn; white unless the block asks: the round
# island's forts each in their own stone)
stone = mul(stone, append(append(custom(3, 1.0), custom(4, 1.0)), custom(5, 1.0)))
stone = mul(stone, lerp(const(1.0), lerp(const(0.72), const(1.0), bevel), sub(const(1.0), solo)))
base = mul(lerp(const3((0.04, 0.037, 0.033)), stone, mortar), dim)
# (dressed stone: its face's relief gentle; at full strength the walls read as raw cliff)
normal = bumped(n, lerp(T, b_al, solo), lerp(B, b_ac, solo), rock_n, 0.4)
finish_n(base, lerp(const(0.97), const(0.82), mortar), normal)
