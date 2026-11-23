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
  M_WaterGlint  the moon's glints on the round island's lake (additive, over its water; WRRoundIsland).
  M_Ice         the ice sheet: deep and clear where it's thick, frosted at the rim, a mirror-smooth top with a cool
                sheen at grazing angles (vertex colour R = thickness, G = 0 at the sunk rim); FarLift as the lava's.
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

# World-space triplanar helpers (the caves' and the rocks' materials).
def tri_weights():
    n = expr(unreal.MaterialExpressionVertexNormalWS)
    a = expr(unreal.MaterialExpressionAbs)
    link(n, a)
    p = mul(mul(a, a), mul(a, a))
    s = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionAdd, mask(p, "r"), mask(p, "g")), mask(p, "b"))
    return n, op(unreal.MaterialExpressionDivide, p, s)


def append(a, b):
    return op(unreal.MaterialExpressionAppendVector, a, b)


def add(a, b):
    return op(unreal.MaterialExpressionAdd, a, b)


def tri_uvs(wp, scale_cm):
    x, y, z = mask(wp, "r"), mask(wp, "g"), mask(wp, "b")
    return mul(append(y, z), k=1.0 / scale_cm), mul(append(x, z), k=1.0 / scale_cm), mul(append(x, y), k=1.0 / scale_cm)


def tri_colour(path, uvs, w):
    cx, cy, cz = [sample(path, uv) for uv in uvs]
    return add(add(mul(cx, mask(w, "r")), mul(cy, mask(w, "g"))), mul(cz, mask(w, "b")))


def tri_bump(path, uvs, w, strength):
    """The texture's normal from the three projections as a world-space offset to add to the surface's normal."""
    tx, ty, tz = [mask(sample(path, uv), "r", "g") for uv in uvs]
    zero = expr(unreal.MaterialExpressionConstant, r=0.0)
    vx = append(zero, tx)                                   # (0, u, v): the X projection runs (Y, Z)
    vy = append(append(mask(ty, "r"), zero), mask(ty, "g"))  # (u, 0, v)
    vz = append(tz, zero)                                   # (u, v, 0)
    return mul(add(add(mul(vx, mask(w, "r")), mul(vy, mask(w, "g"))), mul(vz, mask(w, "b"))), k=strength)


def vc_alpha(vc):
    """The vertex colour's alpha (its first output is RGB only: masking it for A fails to compile)."""
    e = expr(unreal.MaterialExpressionComponentMask, r=True)
    link(vc, e, "", "A")
    return e


def normalized(v):
    e = expr(unreal.MaterialExpressionNormalize)
    link(v, e, "VectorInput")
    return e


def lin(rgb):
    return const3(tuple((c / 255.0) ** 2.2 for c in rgb))


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
    # 11-22 ("everything as good as the caves"): projected along all three axes (blended by the surface's own normal, as
    # the caves'), with a broad tone variation so a field of the same six meshes doesn't repeat, and three optional
    # coats on the faces turned up: Snow (the tundra's rocks), Dust (the volcanic ash settled on them), Lichen (patches
    # of pale sage and orange crust; the lichen node). All 0 by default: the boulders as before, only less repetitive.
    mat.set_editor_property("tangent_space_normal", False)
    wp = expr(unreal.MaterialExpressionWorldPosition)
    n, w = tri_weights()
    uvs = tri_uvs(wp, 300.0)
    rock = tri_colour("Ground/TEX_MWAM_Rock_col", uvs, w)
    tint_p = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Tint", default_value=unreal.LinearColor(0.5, 0.5, 0.5, 1))
    side = append(add(mask(wp, "r"), mask(wp, "g")), mask(wp, "b"))
    macro = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 1800.0)), "r")
    base = mul(mul(mask(rock, "r", "g", "b"), tint_p), lerp(None, None, macro, const_a=0.78, const_b=1.18))
    rock_n = tri_bump("Ground/TEX_MWAM_Rock_nrm", uvs, w, 1.0)
    up = sat(mul(op(unreal.MaterialExpressionSubtract, mask(n, "b"), const_b=0.45), k=3.0))
    breakup = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 240.0)), "b")
    fine = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 45.0)), "g")
    scalar = lambda name: expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=0.0)
    # Dust: pale grey ash in the hollows of the up faces.
    dust = sat(mul(mul(up, scalar("Dust")), lerp(None, None, breakup, const_a=0.5, const_b=1.4)))
    base = lerp(base, lin((92, 88, 84)), mul(dust, k=0.75))
    # Lichen: blotches (where the breakup noise is high) on faces turned up or out, pale sage with orange spots.
    blotch = sat(mul(op(unreal.MaterialExpressionSubtract, add(breakup, mul(fine, k=0.35)), const_b=0.72), k=6.0))
    lichen = mul(mul(blotch, scalar("Lichen")), sat(add(up, expr(unreal.MaterialExpressionConstant, r=0.35))))
    lichen_c = lerp(lin((150, 160, 118)), lin((176, 104, 40)), sat(mul(op(unreal.MaterialExpressionSubtract, fine, const_b=0.78), k=8.0)))
    base = lerp(base, lichen_c, lichen)
    # Snow: on the faces turned up, ragged at its edge.
    snow = sat(mul(op(unreal.MaterialExpressionSubtract, add(up, mul(op(unreal.MaterialExpressionSubtract, breakup, const_b=0.5), k=0.6)), const_b=0.35), k=4.0))
    snow = mul(snow, scalar("Snow"))
    snow_n = tri_bump("Ground/TEX_MWAM_Snow_nrm", tri_uvs(wp, 180.0), w, 0.6)
    base = lerp(base, lerp(lin((214, 226, 238)), lin((192, 206, 224)), breakup), snow)
    normal = normalized(add(n, lerp(rock_n, snow_n, snow)))
    rough = lerp(lerp(expr(unreal.MaterialExpressionConstant, r=0.88), expr(unreal.MaterialExpressionConstant, r=0.95), dust),
                 expr(unreal.MaterialExpressionConstant, r=0.75), snow)
    finish(base, normal, rough)

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
    scalar_p = lambda name, v: expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)
    # Its bright streaks only, raised to CrackPower: thin cracks in a dark crust.
    veins = expr(unreal.MaterialExpressionPower)
    link(mask(t1, "g"), veins, "Base")
    if not any(mel.connect_material_expressions(scalar_p("CrackPower", 3.0), "", veins, n) for n in ("Exp", "Exponent")):
        unreal.log_error("[Wildreach] lava: Power exponent not connected")
    plates = mask(t2, "r")
    # e: 0 = cold crust, 1 = molten. Cracks everywhere (dimmer at the rim), the middle of the flow molten in patches.
    e = sat(op(unreal.MaterialExpressionAdd, mul(veins, lerp(None, None, heat, const_a=0.5, const_b=1.3)),
               mul(mul(mul(heat, heat), lerp(None, None, plates, const_a=0.1, const_b=0.9)), scalar_p("MoltenCore", 0.6))))
    colour = lerp(lerp(const3((0.6, 0.02, 0.003)), const3((1.0, 0.18, 0.01)), sat(mul(e, k=1.8))),
                  const3((1.0, 0.45, 0.06)), sat(mul(op(unreal.MaterialExpressionSubtract, e, const_b=0.7), k=3.0)))
    glow_p = scalar_p("Glow", 1.0)
    # On-screen brightness from CrustLevel (cold crust) to HotLevel (molten); the rim (sunk under the ground) fades out.
    level = mul(mul(lerp(scalar_p("CrustLevel", 0.0), scalar_p("HotLevel", 5.0), mul(e, e)), sat(mul(edge, k=1.5))), glow_p)
    steady = expr(unreal.MaterialExpressionEyeAdaptationInverse)
    alpha = scalar_p("Steady", 0.85)  # 1 = the same brightness on screen whatever the exposure; less: brighter at night
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
    crust_p = expr(unreal.MaterialExpressionVectorParameter, parameter_name="CrustColor", default_value=unreal.LinearColor(0.012, 0.01, 0.009, 1))
    crust = mul(crust_p, lerp(None, None, plates, const_a=0.7, const_b=1.3))  # near-black: by day the sun lit it pale
    finish(crust, sample("/Game/LavaMat/Textures/T_LavaMat1_normal", flow), expr(unreal.MaterialExpressionConstant, r=0.92),
           emissive, wpo, expr(unreal.MaterialExpressionConstant, r=0.15))

# ================================================================ M_Ice (the ice sheet)
# 0.7: Raven's note was the ice should be "glassier", drawn like the lava (a sheet on its own 2 m grid that follows the
# ground, AWRDressing::BuildIceSheet). Vertex colour R = thickness (0 at the rim, 1 a few metres in), G = 0 where the
# sheet dips under the ground. Thick ice looks clear and deep (dark teal-blue, with the ground's tone showing through as
# cloudy streaks), the rim frosted white; a mirror-smooth top (roughness ~0.03, strong specular) with faint ripples and
# cracks in its normal, and a cool fresnel sheen at grazing angles so it reads as glass from afar. Opaque: translucent
# sheets this big would sort badly against the snow and cost far more. FarLift as the lava's (the far landscape is coarser).
if want("ice"):
    begin("/Game/Terrain/M_Ice", ISM)
    wp, uv_top, _ = world_uvs(900.0)
    vc = expr(unreal.MaterialExpressionVertexColor)
    thick, edge = mask(vc, "r"), mask(vc, "g")
    scalar_p = lambda name, v: expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)
    var = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(uv_top, k=0.2)), "r")
    cloud = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(uv_top, k=1.7)), "r")
    deep = expr(unreal.MaterialExpressionVectorParameter, parameter_name="DeepColor", default_value=unreal.LinearColor(0.03, 0.12, 0.2, 1))
    clear = expr(unreal.MaterialExpressionVectorParameter, parameter_name="ClearColor", default_value=unreal.LinearColor(0.2, 0.42, 0.55, 1))
    frost = expr(unreal.MaterialExpressionVectorParameter, parameter_name="FrostColor", default_value=unreal.LinearColor(0.78, 0.86, 0.92, 1))
    # Thick: deep with clearer streaks; thin: frost. Frost again in a fringe a little way in from the rim.
    body = lerp(clear, deep, sat(op(unreal.MaterialExpressionAdd, mul(thick, k=1.2), mul(cloud, k=-0.35))))
    rim = sat(mul(op(unreal.MaterialExpressionSubtract, None, thick, const_a=1.0), k=1.0))
    base = lerp(body, frost, sat(op(unreal.MaterialExpressionAdd, mul(mul(rim, rim), k=1.1), mul(var, k=-0.25))))
    base = mul(base, lerp(None, None, var, const_a=0.85, const_b=1.1))
    # Grazing angles: a pale sheen, as glass shows the sky.
    fres = expr(unreal.MaterialExpressionFresnel, exponent=4.0, base_reflect_fraction=0.04)
    sheen = mul(mul(fres, scalar_p("Sheen", 0.35)), const3((0.7, 0.85, 1.0)))
    emissive = mul(sheen, sat(mul(edge, k=1.5)))
    # Mirror-smooth where it's thick, rougher frost at the rim.
    rough = lerp(scalar_p("Smooth", 0.03), scalar_p("FrostRough", 0.45), sat(mul(mul(rim, rim), k=1.2)))
    # The water normal, broad and mostly flattened (full strength it read as a rippling lake): faint frozen ripples.
    flat = scalar_p("Flatness", 0.78)
    normal = lerp(sample("FX/TEX_MW_WaterA_nrm", mul(uv_top, k=0.35)), expr(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 1, 1)), flat)
    far = op(unreal.MaterialExpressionMax, op(unreal.MaterialExpressionSubtract,
             op(unreal.MaterialExpressionDistance, expr(unreal.MaterialExpressionCameraPositionWS), wp), const_b=2000.0), const_b=0.0)
    lift = mul(mul(far, scalar_p("FarLift", 30.0)), k=1.0 / 10000.0)
    wpo = op(unreal.MaterialExpressionAppendVector, expr(unreal.MaterialExpressionConstant2Vector, r=0.0, g=0.0), lift)
    finish(base, normal, rough, emissive, wpo, scalar_p("Specular", 0.8))

# ================================================================ M_StormAuto (Storm peaks landscape)
# cooks/build-biome-lightning.md 2.3. By height and slope (world Z in cm): wind-flattened grey-green grass on the low
# Wind Passes, slate scree and dirt in the Cliff Paths, granite with ochre lichen on the Struck Crags, snow in the
# hollows above 1,200 m, pale sand by the Desert rib (world Y < -5,250 m) and on the coves; cliffs are slate low and
# granite high, projected sideways so gorge walls don't smear. Rain darkens it (parameter Wet 0..1, set at runtime).
if want("storm"):
    begin("/Game/Terrain/M_StormAuto")
    wp, uv_top, uv_rock = world_uvs(700.0)
    z = mask(wp, "b")
    xy = mask(wp, "r", "g")
    uv = mul(xy, k=1.0 / 400.0)
    uv_stone = mul(xy, k=1.0 / 600.0)
    uv_macro = mul(xy, k=1.0 / 11000.0)
    scalar_p = lambda name, v: expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)

    def band(lo, hi):
        return sat(op(unreal.MaterialExpressionDivide, op(unreal.MaterialExpressionSubtract, z, const_b=lo), const_b=hi - lo))

    normal_z = mask(expr(unreal.MaterialExpressionVertexNormalWS), "b")
    rock_a = sat(mul(op(unreal.MaterialExpressionSubtract, None, normal_z, const_a=0.86), k=9.0))   # ~30 deg and up
    var = mask(sample("Extra/TEX_MWAM_Variation_msk", uv_macro), "r")
    var2 = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(xy, k=1.0 / 2300.0)), "r")
    mid = band(18000.0, 32000.0)          # grass gives way to slate scree between 180 and 320 m
    high = band(56000.0, 68000.0)         # granite crags from 560-680 m
    snowy = mul(band(118000.0, 130000.0), sat(mul(op(unreal.MaterialExpressionSubtract, normal_z, const_b=0.9), k=10.0)))
    shore = sat(op(unreal.MaterialExpressionDivide, op(unreal.MaterialExpressionSubtract, None, z, const_a=500.0), const_b=350.0))
    desert = sat(op(unreal.MaterialExpressionDivide, op(unreal.MaterialExpressionSubtract, None, mask(wp, "g"), const_a=-525000.0), const_b=15000.0))
    sandy = sat(op(unreal.MaterialExpressionAdd, shore, desert))
    lichen = sat(mul(op(unreal.MaterialExpressionSubtract, var2, const_b=0.58), k=6.0))

    grass, grass_n = sample("Ground/TEX_MWAM_Grass_col", uv), sample("Ground/TEX_MWAM_Grass_nrm", uv)
    dirt, dirt_n = sample("Ground/TEX_MWAM_Dirt_col", uv), sample("Ground/TEX_MWAM_Dirt_nrm", uv)
    stones, stones_n = sample("Ground/TEX_MWAM_Stones_col", uv_stone), sample("Ground/TEX_MWAM_Stones_nrm", uv_stone)
    sand, sand_n = sample("Ground/TEX_MWAM_SandA_col", uv), sample("Ground/TEX_MWAM_SandA_nrm", uv)
    snow, snow_n = sample("Ground/TEX_MWAM_Snow_col", uv), sample("Ground/TEX_MWAM_Snow_nrm", uv)
    rock, rock_n = sample("Ground/TEX_MWAM_Rock_col", uv_rock), sample("Ground/TEX_MWAM_Rock_nrm", uv_rock)

    tint = lambda layer, rgb: mul(layer, const3(rgb))
    lowland = lerp(tint(grass, (0.62, 0.66, 0.55)), tint(dirt, (0.42, 0.4, 0.36)), sat(mul(op(unreal.MaterialExpressionSubtract, var, const_b=0.62), k=5.0)))
    scree = lerp(tint(stones, (0.33, 0.36, 0.42)), tint(dirt, (0.3, 0.31, 0.33)), var)
    granite = lerp(tint(stones, (0.62, 0.63, 0.64)), const3((0.47, 0.36, 0.1)), mul(lichen, k=0.55))
    ground = lerp(lerp(lowland, scree, mid), granite, high)
    ground = lerp(ground, tint(sand, (0.78, 0.74, 0.66)), sandy)
    ground = lerp(ground, snow, snowy)
    cliff = mul(rock, lerp(const3((0.11, 0.13, 0.16)), const3((0.36, 0.37, 0.39)), high))
    base = mul(lerp(ground, cliff, rock_a), lerp(None, None, var, const_a=0.82, const_b=1.12))
    wet = scalar_p("Wet", 0.0)
    base = mul(base, lerp(None, None, wet, const_a=1.0, const_b=0.55))
    normal = lerp(lerp(lerp(lerp(lerp(grass_n, stones_n, mid), stones_n, high), sand_n, sandy), snow_n, snowy), rock_n, rock_a)
    rough = lerp(lerp(None, None, snowy, const_a=0.9, const_b=0.72), None, wet, const_b=0.35)
    finish(base, normal, rough)

# ================================================================ M_SwampAuto (the Swamp landscape)
# cooks/build-biome-swamp.md 2.2, cooks/biome-swamp.md 2.2 palettes. By height (world Z cm; the water surface draws at
# +60), slope and region (world X/Y cm): black wet peat mud at the waterline (glossy), olive moss and mud on the low
# ground with garnet bloodmoss patches (#7A1E22, never crimson), reed-gold flats toward the coast (east), pale bone on
# the Bone Mounds' slopes (west, north of y -3,400 m), hoarfrost on the frozen bog (south of y -2,500 m), wet slate
# talus under the Crown with the Weeping Wall's iron-red stain. Rain darkens and glosses it (parameter Wet 0..1).
if want("swamp"):
    begin("/Game/Terrain/M_SwampAuto")
    wp, uv_top, uv_rock = world_uvs(600.0)
    z = mask(wp, "b")
    wx, wy = mask(wp, "r"), mask(wp, "g")
    xy = mask(wp, "r", "g")
    uv = mul(xy, k=1.0 / 350.0)
    uv_stone = mul(xy, k=1.0 / 500.0)
    uv_macro = mul(xy, k=1.0 / 7000.0)
    scalar_p = lambda name, v: expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)

    def ramp(src, lo, hi):
        return sat(op(unreal.MaterialExpressionDivide, op(unreal.MaterialExpressionSubtract, src, const_b=lo), const_b=hi - lo))

    normal_z = mask(expr(unreal.MaterialExpressionVertexNormalWS), "b")
    rock_a = sat(mul(op(unreal.MaterialExpressionSubtract, None, normal_z, const_a=0.8), k=8.0))   # talus and the cliff foot
    var = mask(sample("Extra/TEX_MWAM_Variation_msk", uv_macro), "r")
    var2 = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(xy, k=1.0 / 1900.0)), "r")
    mudline = sat(op(unreal.MaterialExpressionSubtract, None, ramp(z, 80.0, 118.0), const_a=1.0))      # 1 at the waterline
    reeds = ramp(wx, 245000.0, 275000.0)                                                                    # the coast band
    bones = mul(mul(sat(op(unreal.MaterialExpressionSubtract, None, ramp(wx, 140000.0, 175000.0), const_a=1.0)),
                    sat(op(unreal.MaterialExpressionSubtract, None, ramp(wy, -345000.0, -330000.0), const_a=1.0))), ramp(z, 260.0, 420.0))
    frost = ramp(wy, -255000.0, -240000.0)
    bloodmoss = mul(sat(mul(op(unreal.MaterialExpressionSubtract, var2, const_b=0.6), k=6.0)), sat(op(unreal.MaterialExpressionSubtract, None, reeds, const_a=1.0)))
    iron = mul(mul(sat(op(unreal.MaterialExpressionSubtract, None, ramp(wx, 110000.0, 130000.0), const_a=1.0)),
                   mul(ramp(wy, -435000.0, -425000.0), sat(op(unreal.MaterialExpressionSubtract, None, ramp(wy, -352000.0, -342000.0), const_a=1.0)))), rock_a)

    grass, grass_n = sample("Ground/TEX_MWAM_Grass_col", uv), sample("Ground/TEX_MWAM_Grass_nrm", uv)
    dirt, dirt_n = sample("Ground/TEX_MWAM_Dirt_col", uv), sample("Ground/TEX_MWAM_Dirt_nrm", uv)
    stones, stones_n = sample("Ground/TEX_MWAM_Stones_col", uv_stone), sample("Ground/TEX_MWAM_Stones_nrm", uv_stone)
    snow, snow_n = sample("Ground/TEX_MWAM_Snow_col", uv), sample("Ground/TEX_MWAM_Snow_nrm", uv)
    rock, rock_n = sample("Ground/TEX_MWAM_Rock_col", uv_rock), sample("Ground/TEX_MWAM_Rock_nrm", uv_rock)

    tint = lambda layer, rgb: mul(layer, const3(rgb))
    moss = lerp(tint(grass, (0.27, 0.31, 0.17)), tint(dirt, (0.22, 0.19, 0.15)), sat(mul(op(unreal.MaterialExpressionSubtract, var, const_b=0.55), k=4.0)))
    ground = lerp(moss, tint(grass, (0.62, 0.52, 0.29)), reeds)                       # reed gold #B49A5A on the flats
    ground = lerp(ground, tint(grass, (0.42, 0.07, 0.08)), mul(bloodmoss, k=0.85))     # garnet bloodmoss
    ground = lerp(ground, tint(stones, (0.82, 0.79, 0.7)), bones)                      # bone #D6CDB8
    ground = lerp(ground, tint(dirt, (0.13, 0.11, 0.09)), mudline)                     # black peat mud at the water
    ground = lerp(ground, tint(snow, (0.86, 0.9, 0.94)), mul(frost, k=0.7))            # hoarfrost on the frozen bog
    cliff = lerp(tint(rock, (0.16, 0.17, 0.19)), tint(rock, (0.6, 0.23, 0.13)), iron)   # wet slate; rust-red under the falls
    base = mul(lerp(ground, cliff, rock_a), lerp(None, None, var, const_a=0.85, const_b=1.1))
    wet = scalar_p("Wet", 0.0)
    base = mul(base, lerp(None, None, wet, const_a=1.0, const_b=0.6))
    normal = lerp(lerp(lerp(lerp(grass_n, dirt_n, mudline), stones_n, bones), snow_n, mul(frost, k=0.7)), rock_n, rock_a)
    rough = lerp(lerp(None, None, mudline, const_a=0.88, const_b=0.3), None, wet, const_b=0.3)
    finish(base, normal, rough)


# ================================================================ cave walls (WRLandCaves)
# 11-21 (Raven: "do the caves until they look perfect"). The round island's underground caves are one runtime mesh with
# no tangents, so both materials light world-space normals made here: the texture's normal taken from three world
# projections (X, Y and Z, blended by the surface's own normal) and added to the surface's normal (tangent_space_normal
# off). The vertex colour carries the colour the mesh worked out (RGB) and its roughness (A).


if want("caverock"):
    # The Volcanic's lava tubes: dark basalt, glazed where the flow left it (the colour and gloss come from the mesh),
    # the MW rock's grain at two scales, and the flow lines the lava wrote along the walls as it fell: faint level
    # lines at its old levels (UV1.x = the height above the passage's floor, m).
    begin("/Game/Terrain/M_CaveRock")
    mat.set_editor_property("tangent_space_normal", False)
    wp = expr(unreal.MaterialExpressionWorldPosition)
    n, w = tri_weights()
    big, small = tri_uvs(wp, 260.0), tri_uvs(wp, 85.0)
    grain = lerp(tri_colour("Ground/TEX_MWAM_Rock_col", big, w), tri_colour("Ground/TEX_MWAM_Rock_col", small, w), expr(unreal.MaterialExpressionConstant, r=0.45))
    lum = expr(unreal.MaterialExpressionDotProduct)
    link(grain, lum, "A")
    link(const3((0.3, 0.59, 0.11)), lum, "B")
    vc = expr(unreal.MaterialExpressionVertexColor)
    uv1 = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=1)
    warp = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(append(add(mask(wp, "r"), mask(wp, "g")), mask(wp, "b")), k=1.0 / 1400.0)), "r")
    f = expr(unreal.MaterialExpressionFrac)
    link(add(mul(mask(uv1, "r"), k=1.0 / 0.42), mul(warp, k=0.8)), f)
    tri_wave = expr(unreal.MaterialExpressionAbs)
    link(op(unreal.MaterialExpressionSubtract, mul(f, k=2.0), const_b=1.0), tri_wave)
    lines = sat(mul(op(unreal.MaterialExpressionSubtract, tri_wave, const_b=0.82), k=5.5))
    # Only on the walls, from knee to head height (over the roof they drew rings like a tree's; on the floor a maze).
    nz = expr(unreal.MaterialExpressionAbs)
    link(mask(n, "b"), nz)
    h = mask(uv1, "r")
    lines = mul(mul(lines, sat(mul(op(unreal.MaterialExpressionSubtract, None, nz, const_a=0.5), k=4.0))),
                mul(sat(mul(op(unreal.MaterialExpressionSubtract, h, const_b=0.3), k=3.0)), sat(mul(op(unreal.MaterialExpressionSubtract, None, h, const_a=3.0), k=2.0))))
    base = mul(mul(mask(vc, "r", "g", "b"), mul(lum, k=1.6)), add(mul(lines, k=0.3), expr(unreal.MaterialExpressionConstant, r=1.0)))
    # Glazed rock is satin, not a mirror: the near lights silvered whole walls at the mesh's own roughness.
    rough = lerp(expr(unreal.MaterialExpressionConstant, r=0.55), expr(unreal.MaterialExpressionConstant, r=1.0), vc_alpha(vc))
    normal = normalized(add(add(n, tri_bump("Ground/TEX_MWAM_Rock_nrm", big, w, 1.0)), tri_bump("Ground/TEX_MWAM_Rock_nrm", small, w, 0.55)))
    # Standing water (UV1.y = how far under a puddle's waterline + 0.3 m; the mesh changes it linearly across each
    # triangle, so here the level water's edge falls exactly where it meets the rock): a wet band just over the line, and
    # under it the rock dark through the water beneath a level, mirror-smooth top (its normal straight up whatever the
    # floor's bumps: it mirrors the cave as water does; bumpy, wet rock was a glitter of sparks, a burnt patch).
    wl = mask(uv1, "g")
    sub = unreal.MaterialExpressionSubtract
    var = "Extra/TEX_MWAM_Variation_msk"
    wxy = mul(mask(wp, "r", "g"), k=0.01)                  # world metres
    # (the edge wanders a few cm with two scales of noise, as water creeps into the rock's grain)
    rag = add(mul(op(sub, mask(sample(var, mul(wxy, k=1.0 / 0.7)), "g"), const_b=0.5), k=0.03),
              mul(op(sub, mask(sample(var, mul(wxy, k=1.0 / 0.22)), "b"), const_b=0.5), k=0.016))
    under = add(op(sub, wl, const_b=0.3), rag)
    water = sat(mul(under, k=60.0))
    rim = sat(mul(add(under, expr(unreal.MaterialExpressionConstant, r=0.05)), k=20.0))
    deep = sat(mul(under, k=4.0))
    base = mul(base, op(sub, None, mul(rim, k=0.35), const_a=1.0))
    # (clear water: the rock under it a little darker, darker again where it's deep)
    base = lerp(base, mul(base, lerp(None, None, deep, const_a=0.72, const_b=0.42)), water)
    # (the wet band satin, not glossy: at 0.3 the lights' glints along it read as a fringe of frost)
    rough = lerp(lerp(rough, expr(unreal.MaterialExpressionConstant, r=0.5), rim), expr(unreal.MaterialExpressionConstant, r=0.02), water)
    normal = normalized(lerp(normal, const3((0.0, 0.0, 1.0)), water))
    # Seams still glowing (UV2 = heat round a crack's zig-zag, the signed distance across it in m): the fissure itself, a
    # jagged line exactly along the zig-zag (the distance bent by noise), fine where the heat is low and wider at its
    # middle; round it the cooling crust split in a net of polygon cracks (Voronoi cell edges); a dim glow on the crust
    # beside the fissure. Deep red to orange to yellow, held at a steady brightness on screen as the lava flow's is. (A
    # lit strip laid on the floor read as a neon tube; the lava texture's veins as a blurred red smear.)
    uv2 = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=2)
    heat, sd = mask(uv2, "r"), mask(uv2, "g")
    sdw = add(add(sd, mul(op(sub, mask(sample(var, mul(wxy, k=1.0 / 0.9)), "r"), const_b=0.5), k=0.22)),
              mul(op(sub, mask(sample(var, mul(wxy, k=1.0 / 0.25)), "g"), const_b=0.5), k=0.07))
    asd = expr(unreal.MaterialExpressionAbs)
    link(sdw, asd)
    width = lerp(None, None, heat, const_a=0.006, const_b=0.04)
    core = sat(op(sub, None, op(unreal.MaterialExpressionDivide, asd, width), const_a=1.0))
    halo = sat(op(sub, None, mul(asd, k=1.0 / 0.3), const_a=1.0))
    net = expr(unreal.MaterialExpressionCustom, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, description="CrackNet", code="""
float2 Q = P - 512.0 * floor(P / 512.0);
float2 I = floor(Q), F = frac(Q);
float D1 = 8.0, D2 = 8.0;
for (int y = -1; y <= 1; y++)
for (int x = -1; x <= 1; x++)
{
    float2 G = float2(x, y);
    uint2 H = uint2(I + G + 1024.0);
    uint N = H.x * 73856093u ^ H.y * 19349663u;
    N = (N ^ 61u) ^ (N >> 16); N *= 9u; N = N ^ (N >> 4); N *= 0x27d4eb2du; N = N ^ (N >> 15);
    float2 R = G + float2(N & 0xffffu, N >> 16) / 65535.0 - F;
    float D = dot(R, R);
    if (D < D1) { D2 = D1; D1 = D; } else if (D < D2) { D2 = D; }
}
return sqrt(D2) - sqrt(D1);
""")
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", "P")
    net.set_editor_property("inputs", [ci])
    warped = add(mul(wxy, k=1.0 / 0.42), mul(mask(sample(var, mul(wxy, k=1.0 / 1.3)), "r", "g"), k=0.6))
    link(warped, net, "P")
    nline = sat(op(sub, None, mul(net, k=1.0 / 0.07), const_a=1.0))
    e = sat(mul(add(add(mul(core, core), mul(mul(nline, nline), mul(sat(mul(op(sub, heat, const_b=0.35), k=2.5)), k=0.55))),
                    mul(mul(halo, halo), k=0.22)), mul(heat, k=1.3)))
    ember = lerp(lerp(const3((0.6, 0.02, 0.003)), const3((1.0, 0.18, 0.01)), sat(mul(e, k=1.8))),
                 const3((1.0, 0.45, 0.06)), sat(mul(op(sub, e, const_b=0.7), k=3.0)))
    level = mul(mul(e, e), expr(unreal.MaterialExpressionScalarParameter, parameter_name="EmberLevel", default_value=8.0))
    steady = expr(unreal.MaterialExpressionEyeAdaptationInverse)
    ok_l = any(mel.connect_material_expressions(level, "", steady, p) for p in ("LightValue", "Light Value", "LightValueInput"))
    ok_a = any(mel.connect_material_expressions(expr(unreal.MaterialExpressionConstant, r=0.85), "", steady, p) for p in ("Alpha", "AlphaInput"))
    if not (ok_l and ok_a):
        unreal.log_error(f"[Wildreach] caverock: EyeAdaptationInverse inputs not connected ({ok_l} {ok_a})")
    # (the crust darker in the cracks' lines: they're gaps)
    base = mul(base, op(sub, None, mul(sat(mul(add(core, mul(nline, sat(mul(op(sub, heat, const_b=0.35), k=2.5)))), k=1.0)), k=0.8), const_a=1.0))
    finish(base, normal, rough, emissive=mul(ember, steady),
           specular=lerp(expr(unreal.MaterialExpressionConstant, r=0.35), expr(unreal.MaterialExpressionConstant, r=0.3), water))

if want("caveice"):
    # The Tundra's glacier ice: level strata of pale and deep blue drawn here per pixel (bent by noise; done per vertex
    # they broke into sharp triangles), deeper blue knots, now and then a dirty band of grit; scalloped like the walls
    # of a real ice cave (the water ripple's normal at two scales); snow where the mesh says (UV1.x = ice 0..1: 0 is the
    # vertex colour's snow or ground). Where the ice over you is thin it glows with the day (UV1.y = thin, the "Glow"
    # parameter = daylight); deep in it darkens (UV2.x).
    begin("/Game/Terrain/M_CaveIce")
    mat.set_editor_property("tangent_space_normal", False)
    wp = expr(unreal.MaterialExpressionWorldPosition)
    n, w = tri_weights()
    vc = expr(unreal.MaterialExpressionVertexColor)
    uv1 = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=1)
    uv2 = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=2)
    ice, thin, dark, clear = mask(uv1, "r"), mask(uv1, "g"), mask(uv2, "r"), mask(uv2, "g")
    side = append(add(mask(wp, "r"), mask(wp, "g")), mask(wp, "b"))
    warp = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 1500.0)), "r")
    knots = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 420.0)), "g")
    f = expr(unreal.MaterialExpressionFrac)
    link(add(mul(mask(wp, "b"), k=1.0 / 115.0), mul(warp, k=1.4)), f)
    tri_wave = expr(unreal.MaterialExpressionAbs)
    link(op(unreal.MaterialExpressionSubtract, mul(f, k=2.0), const_b=1.0), tri_wave)
    strata = sat(mul(op(unreal.MaterialExpressionSubtract, tri_wave, const_b=0.25), k=2.0))
    ice_c = lerp(lin((70, 128, 196)), lin((176, 214, 240)), strata)
    ice_c = lerp(ice_c, lin((40, 92, 170)), mul(sat(mul(op(unreal.MaterialExpressionSubtract, knots, const_b=0.55), k=3.0)), k=0.45))
    fd = expr(unreal.MaterialExpressionFrac)
    link(add(mul(mask(wp, "b"), k=1.0 / 380.0), mul(warp, k=0.7)), fd)
    dirt = sat(mul(op(unreal.MaterialExpressionSubtract, fd, const_b=0.93), k=14.0))
    ice_c = lerp(ice_c, lin((92, 96, 102)), mul(dirt, k=0.55))
    ice_c = mul(ice_c, op(unreal.MaterialExpressionSubtract, None, mul(dark, k=0.62), const_a=1.0))
    # Clear ice (UV2.y: icicles, frozen falls): pale and glassy, not the walls' strata.
    ice_c = lerp(ice_c, lin((196, 224, 244)), clear)
    base = lerp(mask(vc, "r", "g", "b"), ice_c, ice)
    # (Wet ice, not foil: at 0.2 every dimple in a narrow way caught a glint.)
    rough = lerp(vc_alpha(vc), expr(unreal.MaterialExpressionConstant, r=0.32), ice)
    # Scallops: the dimpled walls of a real ice cave, the stones' rounded bumps turned inside out (negative strength),
    # over a broad undulation; on clear ice only a little (the ripple's lines ran like fur over the walls).
    # (At 1.3 m the stones' bumps were a speckle in the narrow ways: 3.6 m tiles make them dimples a hand or two wide.)
    big, small, snow_uv = tri_uvs(wp, 360.0), tri_uvs(wp, 520.0), tri_uvs(wp, 190.0)
    ice_bump = mul(add(tri_bump("Ground/TEX_MWAM_Stones_nrm", big, w, -0.22), tri_bump("Ground/TEX_MWAM_Rock_nrm", small, w, 0.12)),
                   op(unreal.MaterialExpressionSubtract, None, mul(clear, k=0.7), const_a=1.0))
    normal = normalized(add(n, lerp(tri_bump("Ground/TEX_MWAM_Snow_nrm", snow_uv, w, 0.7), ice_bump, ice)))
    glow_p = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Glow", default_value=0.0)
    emissive = mul(mul(mul(lerp(lin((60, 140, 230)), lin((150, 205, 245)), strata), thin), ice), glow_p)
    finish(base, normal, rough, emissive)

if want("iceblock"):
    # 11-22 ("everything as good as the caves"): glacier ice for loose ice in the open and in the Blue Below's corridor
    # (seracs, ice boulders, the tube's walls; they were the engine's flat shape material in pale blue). The caves' ice
    # without their vertex data: level strata of pale and deep blue bent by noise, deeper knots, now and then a band of
    # grit, scalloped; snow lying on the faces turned up (and everywhere with "Snow" 1: the drifts). "Tint" shifts it.
    begin("/Game/Terrain/M_IceBlock", ISM)
    mat.set_editor_property("tangent_space_normal", False)
    wp = expr(unreal.MaterialExpressionWorldPosition)
    n, w = tri_weights()
    side = append(add(mask(wp, "r"), mask(wp, "g")), mask(wp, "b"))
    warp = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 1500.0)), "r")
    knots = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 420.0)), "g")
    f = expr(unreal.MaterialExpressionFrac)
    link(add(mul(mask(wp, "b"), k=1.0 / 95.0), mul(warp, k=1.4)), f)
    tri_wave = expr(unreal.MaterialExpressionAbs)
    link(op(unreal.MaterialExpressionSubtract, mul(f, k=2.0), const_b=1.0), tri_wave)
    strata = sat(mul(op(unreal.MaterialExpressionSubtract, tri_wave, const_b=0.25), k=2.0))
    # (glacier ice is mostly white and pale cyan, deep blue only in its hollows: saturated, it read as blue plastic)
    ice_c = lerp(lin((150, 188, 214)), lin((214, 230, 242)), strata)
    ice_c = lerp(ice_c, lin((92, 146, 198)), mul(sat(mul(op(unreal.MaterialExpressionSubtract, knots, const_b=0.6), k=3.0)), k=0.4))
    fd = expr(unreal.MaterialExpressionFrac)
    link(add(mul(mask(wp, "b"), k=1.0 / 330.0), mul(warp, k=0.7)), fd)
    dirt = sat(mul(op(unreal.MaterialExpressionSubtract, fd, const_b=0.93), k=14.0))
    ice_c = lerp(ice_c, lin((96, 100, 104)), mul(dirt, k=0.5))
    ice_c = mul(ice_c, expr(unreal.MaterialExpressionVectorParameter, parameter_name="Tint", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)))
    big, small, snow_uv = tri_uvs(wp, 300.0), tri_uvs(wp, 460.0), tri_uvs(wp, 160.0)
    ice_bump = add(tri_bump("Ground/TEX_MWAM_Stones_nrm", big, w, -0.24), tri_bump("Ground/TEX_MWAM_Rock_nrm", small, w, 0.14))
    snow_bump = tri_bump("Ground/TEX_MWAM_Snow_nrm", snow_uv, w, 0.7)
    up = sat(mul(op(unreal.MaterialExpressionSubtract, mask(n, "b"), const_b=0.55), k=4.0))
    breakup = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 260.0)), "b")
    snow_amt = sat(add(add(mul(up, lerp(None, None, breakup, const_a=0.7, const_b=1.3)),
                         expr(unreal.MaterialExpressionScalarParameter, parameter_name="Snow", default_value=0.0)), expr(unreal.MaterialExpressionConstant, r=0.0)))
    snow_c = lerp(lin((214, 226, 238)), lin((192, 206, 224)), breakup)
    base = lerp(ice_c, snow_c, snow_amt)
    rough = lerp(expr(unreal.MaterialExpressionConstant, r=0.38), expr(unreal.MaterialExpressionConstant, r=0.82), snow_amt)
    normal = normalized(add(n, lerp(ice_bump, snow_bump, snow_amt)))
    finish(base, normal, rough, specular=expr(unreal.MaterialExpressionConstant, r=0.45))

if want("obsidian"):
    # 11-22: volcanic glass for the obsidian blades (they were boulders tinted black, matte): near black, glossy, the faint
    # ripples of a conchoidal fracture in its relief, a smoky brown where it's thin at grazing angles.
    begin("/Game/Terrain/M_Obsidian", ISM)
    mat.set_editor_property("tangent_space_normal", False)
    wp = expr(unreal.MaterialExpressionWorldPosition)
    n, w = tri_weights()
    ripples = tri_bump("Ground/TEX_MWAM_Stones_nrm", tri_uvs(wp, 140.0), w, 0.22)
    normal = normalized(add(n, ripples))
    fres = expr(unreal.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.0)
    base = lerp(lin((10, 10, 13)), lin((34, 26, 22)), mul(fres, k=0.6))
    side = append(add(mask(wp, "r"), mask(wp, "g")), mask(wp, "b"))
    var = mask(sample("Extra/TEX_MWAM_Variation_msk", mul(side, k=1.0 / 300.0)), "r")
    rough = lerp(None, None, var, const_a=0.06, const_b=0.2)
    finish(base, normal, rough, specular=expr(unreal.MaterialExpressionConstant, r=0.6))

if want("glint"):
    # 11-22 round 7 (the look pass: at night the lake didn't mirror the moon). The lake's water is the WaterMaterials ocean:
    # translucent, lit per vertex through the volume, its highlight a painted texture, its reflections a fixed cubemap, so
    # no light's highlight ever shows on it. This lies on the lake a centimetre over its surface (WRRoundIsland, the
    # lake's outline), additive and unlit: per pixel the view reflected off a rippled surface (six wave trains, each one
    # fading out where it gets finer than a pixel so the far water doesn't shimmer, its slope then widening the highlight
    # instead), against the light's direction: the moon's path of glints on the water and a soft glow round it, the
    # water's Fresnel (bright low across it, faint looking down). Parameters (WRRoundIsland, from WRSky): LightDir (toward
    # the light), LightColour, Strength (0 by day / under cloud), Level, Sharp (the highlight's exponent), Ripple (the
    # waves' slope). Held at a steady on-screen brightness, as the stars are.
    begin("/Game/Terrain/M_WaterGlint")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", False)
    glint = expr(unreal.MaterialExpressionCustom, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Glint", code="""
float3 Vd = -normalize(CV);
float2 g = 0;
float lost = 0;
// The water's position bent a little (slow, wandering): the wave trains don't line up into a lattice.
float2 W = P.xy + 70.0 * float2(sin(P.y * 0.0061 + T * 0.05 + sin(P.x * 0.0043)), sin(P.x * 0.0057 - T * 0.04 + sin(P.y * 0.0049)))
             + 18.0 * float2(sin(P.y * 0.021 + sin(P.x * 0.017 + T * 0.1)), sin(P.x * 0.019 + sin(P.y * 0.023 - T * 0.08)));
const float2 D[8] = { float2(0.80, 0.60), float2(-0.45, 0.89), float2(0.97, -0.24), float2(-0.88, -0.47), float2(0.20, 0.98), float2(-0.62, 0.78), float2(0.41, -0.91), float2(-0.99, 0.12) };
const float Lm[8] = { 310.0, 213.0, 141.0, 97.0, 66.0, 43.0, 31.0, 23.0 };
const float Sp[8] = { 0.9, 1.3, 1.1, 1.7, 2.1, 2.6, 2.9, 3.3 };
for (int i = 0; i < 8; i++)
{
    float k = 6.2832 / Lm[i];
    float ph = dot(D[i], W) * k + T * Sp[i] * 1.6;
    float px = length(float2(ddx(ph), ddy(ph)));
    float keep = saturate(1.5 - px / 1.4);
    // each train swells and fades in patches across the water
    float sw = 0.55 + 0.45 * sin(dot(float2(-D[i].y, D[i].x), P.xy) * 0.0045 + i * 1.7 + sin(dot(D[i], P.xy) * 0.0031 + i));
    g += D[i] * (A * keep * sw * cos(ph));
    lost += (1.0 - keep) * sw;
}
lost *= 0.75;
float3 N = normalize(float3(-g.x, -g.y, 1.0));
float3 R = reflect(Vd, N);
float c = saturate(dot(R, normalize(L)));
float rough = A * sqrt(lost / 8.0) * 2.0;
float k2 = K / (1.0 + K * rough * rough);
float spec = pow(c, k2) * sqrt(k2 / K);
float haze = pow(c, 36.0) * 0.035;
float fres = 0.02 + 0.98 * pow(1.0 - saturate(dot(N, -Vd)), 5.0);
return Col * fres * (spec * 14.0 + haze);
""")
    ins = []
    for name in ("CV", "P", "T", "L", "Col", "A", "K"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    glint.set_editor_property("inputs", ins)
    link(expr(unreal.MaterialExpressionCameraVectorWS), glint, "CV")
    link(expr(unreal.MaterialExpressionWorldPosition), glint, "P")
    link(expr(unreal.MaterialExpressionTime), glint, "T")
    link(expr(unreal.MaterialExpressionVectorParameter, parameter_name="LightDir", default_value=unreal.LinearColor(0.0, -0.6, 0.2, 1.0)), glint, "L")
    link(expr(unreal.MaterialExpressionVectorParameter, parameter_name="LightColour", default_value=unreal.LinearColor(0.62, 0.72, 1.0, 1.0)), glint, "Col")
    link(expr(unreal.MaterialExpressionScalarParameter, parameter_name="Ripple", default_value=0.07), glint, "A")
    link(expr(unreal.MaterialExpressionScalarParameter, parameter_name="Sharp", default_value=1400.0), glint, "K")
    strength = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Strength", default_value=0.0)
    level = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Level", default_value=1.0)
    steady = expr(unreal.MaterialExpressionEyeAdaptationInverse)
    ok_l = any(mel.connect_material_expressions(level, "", steady, p) for p in ("LightValue", "Light Value", "LightValueInput"))
    ok_a = any(mel.connect_material_expressions(expr(unreal.MaterialExpressionConstant, r=0.8), "", steady, p) for p in ("Alpha", "AlphaInput"))
    if not (ok_l and ok_a):
        unreal.log_error(f"[Wildreach] glint: EyeAdaptationInverse inputs not connected ({ok_l} {ok_a})")
    # only over water deep enough to show it: the sheet runs a little past the shore, and over the shallow sand at your
    # feet (the water's own opacity fades there) the glints lay on the beach (1.2 m of water to come in full)
    depth = op(unreal.MaterialExpressionSubtract, expr(unreal.MaterialExpressionSceneDepth), expr(unreal.MaterialExpressionPixelDepth))
    deep = sat(mul(depth, k=1.0 / 120.0))
    mel.connect_material_property(mul(mul(mul(glint, strength), steady), deep), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")

if want("nightsky"):
    # 11-22 (the whole-game look pass: the night had no stars): the stars on a dome round the camera (WRSky BuildStars, a
    # 60 km sphere so the clouds draw over them: Apply Cloud Fogging), additive and unlit, only where the view meets the
    # sky (the scene's depth past 40 km). A hashed star field in two layers (many faint, a few bright, each its own
    # tint, a little twinkle), kept about a pixel across at any distance, a faint Milky Way band, all fading into the
    # haze near the horizon. Directions are the dome's own, so turning the dome (the hours) turns the sky. Parameters:
    # Night (0..1, WRSky by the sun's depth and the weather), StarLevel. Held at a steady on-screen brightness.
    begin("/Game/Terrain/M_NightSky")
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("disable_depth_test", True)
    for prop in ("apply_cloud_fogging", "apply_fogging"):
        try:
            mat.set_editor_property(prop, prop == "apply_cloud_fogging")
        except Exception as ex:
            unreal.log_warning(f"[Wildreach] nightsky: no {prop} ({ex})")
    cam = expr(unreal.MaterialExpressionCameraVectorWS)
    d_world = mul(cam, k=-1.0)
    d_local = expr(unreal.MaterialExpressionTransform,
                   transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                   transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    link(d_world, d_local)
    stars = expr(unreal.MaterialExpressionCustom, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description="Stars", code="""
float3 d = normalize(D);
float3 col = 0;
for (int L = 0; L < 2; L++)
{
    float sc = L == 0 ? 460.0 : 160.0;
    float dens = L == 0 ? 0.78 : 0.955;
    float3 p = d * sc;
    float3 c = floor(p);
    float3 f = p - c;
    uint3 H = uint3(c + 8192.0);
    uint N = (H.x * 73856093u) ^ (H.y * 19349663u) ^ (H.z * 83492791u);
    N = (N ^ 61u) ^ (N >> 16); N *= 9u; N = N ^ (N >> 4); N *= 0x27d4eb2du; N = N ^ (N >> 15);
    float h = (N & 0xffffu) / 65535.0;
    float h2 = (N >> 16) / 65535.0;
    if (h > dens)
    {
        float3 o = frac(float3(h2 * 13.7, h2 * 71.3, h2 * 197.9)) * 0.6 + 0.2;
        float r = length(f - o);
        float px = max(length(ddx(p)), length(ddy(p)));
        float s = max(px * 1.2, 0.015);
        float b = saturate(1.0 - r / s);
        b *= b;
        float mag = pow((h - dens) / (1.0 - dens), 2.5) * (L == 0 ? 0.6 : 3.0);
        mag *= 0.8 + 0.2 * sin(T * (1.5 + h2 * 4.0) + h2 * 40.0);
        float3 tint = lerp(float3(0.72, 0.82, 1.0), float3(1.0, 0.86, 0.66), frac(h2 * 7.31));
        col += tint * b * mag;
    }
}
float3 A = normalize(float3(0.32, -0.58, 0.75));
float m = dot(d, A);
float band = exp(-m * m / 0.018);
float clump = 0.55 + 0.45 * sin(d.x * 11.0 + sin(d.y * 7.0) * 2.0) * sin(d.y * 9.0 + d.z * 13.0);
col += float3(0.75, 0.8, 1.0) * band * clump * 0.035;
return col * saturate((U - 0.015) / 0.14);
""")
    ins = []
    for name in ("D", "U", "T"):
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        ins.append(ci)
    stars.set_editor_property("inputs", ins)
    link(d_local, stars, "D")
    link(mask(d_world, "b"), stars, "U")
    link(expr(unreal.MaterialExpressionTime), stars, "T")
    depth = expr(unreal.MaterialExpressionSceneDepth)
    sky = sat(mul(op(unreal.MaterialExpressionSubtract, depth, const_b=4.0e6), k=1.0e-6))
    night = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Night", default_value=1.0)
    level = expr(unreal.MaterialExpressionScalarParameter, parameter_name="StarLevel", default_value=2.0)
    steady = expr(unreal.MaterialExpressionEyeAdaptationInverse)
    ok_l = any(mel.connect_material_expressions(mul(night, level), "", steady, p) for p in ("LightValue", "Light Value", "LightValueInput"))
    ok_a = any(mel.connect_material_expressions(expr(unreal.MaterialExpressionConstant, r=0.8), "", steady, p) for p in ("Alpha", "AlphaInput"))
    if not (ok_l and ok_a):
        unreal.log_error(f"[Wildreach] nightsky: EyeAdaptationInverse inputs not connected ({ok_l} {ok_a})")
    mel.connect_material_property(mul(mul(stars, sky), steady), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
