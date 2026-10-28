"""Terrain and prop materials from the art packs (MW Landscape Auto Material, LavaMat). Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/terrain/build_terrain_materials.py"
     WR_MATERIALS=lava (comma list of island, rock, lava, ice) rebuilds only those (each rebuild re-saves the asset).

  M_IslandAuto  rebuilt in place (the landscape references it, so L_Island doesn't need re-saving).
                Volcanic west: dark ash with stony patches, black-sand shore. Tundra east: snow, pale-sand shore.
                Steep slopes: rock (basalt west, frost-grey east), projected sideways so cliffs don't smear.
  M_Rock        boulders (island dressing): MW rock texture projected from above and the side; "Tint" per biome.
  M_Lava        the lava flow: dark crust split by glowing cracks, molten orange toward the middle of the flow (vertex
                colour R = heat, G = 0 at the sunk rim), drifting downstream (west); its glow is held at a steady
                on-screen brightness whatever the exposure, and far lava is lifted over the coarser far landscape.
                Parameters Glow, FarLift (cm per 100 m) are set from tuning (lava.*).
  M_Ice         ice cells: glossy pale blue with ripples.
"""
import os
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
MW = "/Game/MWLandscapeAutoMaterial/Textures/"
mat = None
col = [0]  # layout cursor so the graph is readable if someone opens it
ONLY = [n.strip() for n in os.environ.get("WR_MATERIALS", "").split(",") if n.strip()]


def want(name):
    return not ONLY or name in ONLY


def begin(path, usages=()):
    """Load (or create) a material and clear its graph."""
    global mat
    if eal.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        folder, name = path.rsplit("/", 1)
        mat = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    for u in usages:
        mel.set_base_material_usage(mat, u) if hasattr(mel, "set_base_material_usage") else mel.set_material_usage(mat, u)
    col[0] = 0


def finish(base, normal, rough, emissive=None, wpo=None, specular=None):
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if emissive is not None:
        mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if wpo is not None:
        mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    if specular is not None:
        mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
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


def mul(a, b=None, k=None):
    return op(unreal.MaterialExpressionMultiply, a, b) if k is None else op(unreal.MaterialExpressionMultiply, a, const_b=k)


def mask(src, *chans):
    e = expr(unreal.MaterialExpressionComponentMask, r="r" in chans, g="g" in chans, b="b" in chans, a="a" in chans)
    link(src, e)
    return e


def const3(rgb):
    return expr(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def lerp(a, b, alpha, **consts):
    e = expr(unreal.MaterialExpressionLinearInterpolate, **consts)
    if a is not None:
        link(a, e, "A")
    if b is not None:
        link(b, e, "B")
    link(alpha, e, "Alpha")
    return e


def sat(x):
    e = expr(unreal.MaterialExpressionSaturate)
    link(x, e)
    return e


def sample(path, uv):
    tex = unreal.load_asset(path if path.startswith("/Game/") else MW + path)
    cs = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    elif cs == unreal.TextureCompressionSettings.TC_MASKS:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    elif cs == unreal.TextureCompressionSettings.TC_GRAYSCALE:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE
    else:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    e = expr(unreal.MaterialExpressionTextureSample, texture=tex, sampler_type=st,
             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)  # shared samplers: no 16-sampler limit
    link(uv, e, "UVs")
    return e


def world_uvs(scale_cm):
    """(top, side) world-aligned UVs: XY from above, and a sideways (X+Y, Z) projection for steep faces."""
    wp = expr(unreal.MaterialExpressionWorldPosition)
    top = mul(mask(wp, "r", "g"), k=1.0 / scale_cm)
    side_u = mul(op(unreal.MaterialExpressionAdd, mask(wp, "r"), mask(wp, "g")), k=0.7071)
    side = mul(op(unreal.MaterialExpressionAppendVector, side_u, mask(wp, "b")), k=1.0 / scale_cm)
    return wp, top, side


ISM = (unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,)

# ================================================================ M_IslandAuto (landscape)
if want("island"):
    begin("/Game/Terrain/M_IslandAuto")
    wp, uv_top, uv_rock = world_uvs(600.0)
    z = mask(wp, "b")
    xy = mask(wp, "r", "g")
    uv = mul(xy, k=1.0 / 400.0)          # 4 m ground tiles
    uv_stone = mul(xy, k=1.0 / 650.0)
    uv_macro = mul(xy, k=1.0 / 9000.0)   # 90 m variation

    biome = sat(op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionDivide, mask(wp, "r"), const_b=8000.0), const_b=0.5))
    normal_z = mask(expr(unreal.MaterialExpressionVertexNormalWS), "b")
    rock_a = sat(mul(op(unreal.MaterialExpressionSubtract, None, normal_z, const_a=0.9), k=12.0))
    shore_a = sat(op(unreal.MaterialExpressionDivide, op(unreal.MaterialExpressionSubtract, None, z, const_a=450.0), const_b=300.0))
    var = mask(sample("Extra/TEX_MWAM_Variation_msk", uv_macro), "r")
    stone_a = sat(mul(op(unreal.MaterialExpressionSubtract, var, const_b=0.55), k=5.0))

    dirt, dirt_n = sample("Ground/TEX_MWAM_Dirt_col", uv), sample("Ground/TEX_MWAM_Dirt_nrm", uv)
    stones, stones_n = sample("Ground/TEX_MWAM_Stones_col", uv_stone), sample("Ground/TEX_MWAM_Stones_nrm", uv_stone)
    sand, sand_n = sample("Ground/TEX_MWAM_SandC_col", uv), sample("Ground/TEX_MWAM_SandC_nrm", uv)
    snow, snow_n = sample("Ground/TEX_MWAM_Snow_col", uv), sample("Ground/TEX_MWAM_Snow_nrm", uv)
    rock, rock_n = sample("Ground/TEX_MWAM_Rock_col", uv_rock), sample("Ground/TEX_MWAM_Rock_nrm", uv_rock)

    tint = lambda layer, rgb: mul(layer, const3(rgb))
    volcanic = lerp(lerp(tint(dirt, (0.13, 0.12, 0.115)), tint(stones, (0.17, 0.16, 0.155)), stone_a),
                    tint(sand, (0.09, 0.085, 0.085)), shore_a)                                     # ash, stones, black sand
    tundra = lerp(snow, tint(sand, (0.85, 0.85, 0.82)), shore_a)                                    # snow, pale sand
    ground = lerp(volcanic, tundra, biome)
    rock_c = mul(rock, lerp(const3((0.15, 0.14, 0.135)), const3((0.78, 0.82, 0.88)), biome))
    base = mul(lerp(ground, rock_c, rock_a), lerp(None, None, var, const_a=0.8, const_b=1.12))

    normal = lerp(lerp(lerp(lerp(dirt_n, stones_n, stone_a), sand_n, shore_a), lerp(snow_n, sand_n, shore_a), biome), rock_n, rock_a)
    finish(base, normal, lerp(None, None, biome, const_a=0.93, const_b=0.7))  # snow is a little glossier

# ================================================================ M_Rock (boulders)
if want("rock"):
    # Nanite too: the cave boulders are Nanite meshes, and without the flag cooked builds draw them in the default material.
    begin("/Game/Terrain/M_Rock", ISM + (unreal.MaterialUsage.MATUSAGE_NANITE,))
    wp, uv_top, uv_side = world_uvs(300.0)
    up = sat(mul(mask(expr(unreal.MaterialExpressionVertexNormalWS), "b"), k=1.5))
    top_c, side_c = sample("Ground/TEX_MWAM_Rock_col", uv_top), sample("Ground/TEX_MWAM_Rock_col", uv_side)
    top_n, side_n = sample("Ground/TEX_MWAM_Rock_nrm", uv_top), sample("Ground/TEX_MWAM_Rock_nrm", uv_side)
    tint_p = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Tint", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1))
    finish(mul(lerp(side_c, top_c, up), tint_p), lerp(side_n, top_n, up), expr(unreal.MaterialExpressionConstant, r=0.88))

# ================================================================ M_Lava (the lava flow)
# Raven saw "weird lava squares": flat pale-pink sheets. The old glow was the LavaMat emissive everywhere at x14, which
# auto-exposure (0.4.0: day floor -2 EV) blew out to a pale salmon, plus a glossy black base that mirrored the bright
# sky. Now: a matte dark crust, glowing only along its cracks and toward the flow's molten middle, deep red to orange to
# yellow with heat, and the glow is held at a steady brightness on screen whatever the exposure (EyeAdaptationInverse),
# so it reads as glowing lava by day and doesn't burn out to white at night.
if want("lava"):
    begin("/Game/Terrain/M_Lava", ISM)
    wp, uv_top, _ = world_uvs(700.0)
    vc = expr(unreal.MaterialExpressionVertexColor)
    heat, edge = mask(vc, "r"), mask(vc, "g")
    # Two drifts of the crack pattern (downstream = west), one large and slow, so the tiling doesn't show.
    flow = expr(unreal.MaterialExpressionPanner, speed_x=-0.010, speed_y=0.002)
    link(uv_top, flow, "Coordinate")
    flow2 = expr(unreal.MaterialExpressionPanner, speed_x=-0.004, speed_y=-0.001)
    link(mul(uv_top, k=0.37), flow2, "Coordinate")
    t1 = sample("/Game/LavaMat/Textures/T_LavaMat1_Emissive", flow)
    t2 = sample("/Game/LavaMat/Textures/T_LavaMat1_Emissive", flow2)
    veins = mul(mask(t1, "g"), mask(t1, "g"))                     # its bright streaks only, squared: cracks in a dark crust
    plates = mask(t2, "r")
    # e: 0 = cold crust, 1 = molten. Cracks everywhere (dimmer at the rim), the middle of the flow molten all over.
    e = sat(op(unreal.MaterialExpressionAdd, mul(veins, lerp(None, None, heat, const_a=0.45, const_b=1.25)),
               mul(mul(heat, heat), lerp(None, None, plates, const_a=0.15, const_b=0.75))))
    colour = lerp(lerp(const3((0.85, 0.05, 0.006)), const3((1.0, 0.30, 0.025)), sat(mul(e, k=1.6))),
                  const3((1.0, 0.62, 0.16)), sat(mul(op(unreal.MaterialExpressionSubtract, e, const_b=0.6), k=2.5)))
    glow_p = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=1.0)
    # On-screen brightness: ~0.06 for cold crust up to ~3 for the hottest; the rim (sunk under the ground) fades out.
    level = mul(mul(lerp(None, None, e, const_a=0.06, const_b=3.0), sat(mul(edge, k=1.5))), glow_p)
    steady = expr(unreal.MaterialExpressionEyeAdaptationInverse)
    alpha = expr(unreal.MaterialExpressionConstant, r=0.85)  # mostly steady: a touch brighter at night still
    ok_l = any(mel.connect_material_expressions(level, "", steady, n) for n in ("LightValue", "Light Value", "LightValueInput"))
    ok_a = any(mel.connect_material_expressions(alpha, "", steady, n) for n in ("Alpha", "AlphaInput"))
    if not (ok_l and ok_a):
        unreal.log_error(f"[Wildreach] lava: EyeAdaptationInverse inputs not connected ({ok_l} {ok_a})")
    emissive = mul(colour, steady)
    # Far lava rises a little over the landscape, whose far levels of detail are coarser than the sheet.
    far = op(unreal.MaterialExpressionMax, op(unreal.MaterialExpressionSubtract,
             op(unreal.MaterialExpressionDistance, expr(unreal.MaterialExpressionCameraPositionWS), wp), const_b=2000.0), const_b=0.0)
    farlift_p = expr(unreal.MaterialExpressionScalarParameter, parameter_name="FarLift", default_value=45.0)
    lift = mul(mul(far, farlift_p), k=1.0 / 10000.0)
    wpo = op(unreal.MaterialExpressionAppendVector, expr(unreal.MaterialExpressionConstant2Vector, r=0.0, g=0.0), lift)
    crust = mul(const3((0.03, 0.026, 0.024)), lerp(None, None, plates, const_a=0.7, const_b=1.2))
    finish(crust, sample("/Game/LavaMat/Textures/T_LavaMat1_normal", flow), expr(unreal.MaterialExpressionConstant, r=0.92),
           emissive, wpo, expr(unreal.MaterialExpressionConstant, r=0.15))

# ================================================================ M_Ice (ice cells)
if want("ice"):
    begin("/Game/Terrain/M_Ice", ISM)
    wp, uv_top, _ = world_uvs(900.0)
    var = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(uv_top, k=0.2)), "r")
    base = mul(const3((0.55, 0.74, 0.9)), lerp(None, None, var, const_a=0.75, const_b=1.05))
    finish(base, sample("FX/TEX_MW_WaterA_nrm", uv_top), expr(unreal.MaterialExpressionConstant, r=0.08))
