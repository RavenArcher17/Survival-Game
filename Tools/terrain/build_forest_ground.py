"""The Ancient forest's ground (Aaron 11-16, r-forest-ground: "We already have the electric dreams env pack. Can't we use
that to create an incredible forest biome? Or take some lessons we learned from Trails?"; showcase-notes
cooks/wild-answers-1115.md 2.5). Safe to re-run.

Run (one heavy editor job; all six Fab packs present):
  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/terrain/build_forest_ground.py <scans dir>"
<scans dir> (or the WR_FOREST_SCANS environment variable) holds Trails of Appalachia's terrain sets (TrailsRepo Raw/Art/Terrain: TER_LeafLitter, TER_ForestSoil,
TER_MossGround, each T_TER_<set>_BCH.png = colour + height in alpha and _N.png = DirectX normal; built from the CC0
ambientCG scans Ground053 and kin by Trails' terrain_surfaces.py, tiled at 2.5 m). They are imported at 1K to
/Game/Terrain/Forest (already in the repo: without the dir the import is skipped and the material is rebuilt from them).

  M_ForestGround  the forest land's ground (WRLand chunks; Data/lands/forest.json "groundMaterial"). Six layers mixed
                  per vertex (UV1 litter / moss, UV2 soil / grass, UV3 stones / sand, from FWRLandDef::HfLayers): leaf
                  litter, forest soil and moss from the scans, grass, stones and sand from the MW auto-material pack.
                  Height-blended (each layer's own height decides who shows through at a border: moss fills the
                  litter's hollows, stones stand out of sand) instead of an alpha fade; a 6.1 m second litter scale
                  and a 31 m brightness wash break the tiling; steep ground (33-47 degrees, as the colour's) is the MW
                  rock, projected from the side. At every rock, root, log and trunk base the ground goes to moss and
                  damp shade within 70 cm (the global distance field: Electric Dreams hides contacts with dirt and moss
                  decals; this does it for every mesh at once). The vertex colour (each sub-zone's palette) stays a
                  gentle tint. Parameter: Wet (0..1, the land's rain, as M_LandGround).
  M_IslandGround  the round island's ground (Data/lands/island.json; built with --island): the same layers, but each
                  land's colour comes from the vertex colour (TintAmount 0.8 of it: the textures keep their light and
                  shade, the land its own colour), so one ground can be snow, basalt, dune sand and meadow.
"""
import os
import sys
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(_here, "..", "make_build_materials.py"), encoding="utf-8").read()
exec(_src[: _src.index("# ================================================================ M_BuildWood")])

FOLDER = "/Game/Terrain/Forest"
SETS = {"Litter": "TER_LeafLitter", "Soil": "TER_ForestSoil", "Moss": "TER_MossGround"}
MW = "/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_"


def import_scans(src_dir):
    """The three scan sets as 1K textures: colour + height (sRGB colour, the alpha read as it is) and the normal map."""
    tasks = []
    for short, folder in SETS.items():
        for kind in ("BCH", "N"):
            path = os.path.join(src_dir, folder, f"T_{folder}_{kind}.png")
            if not os.path.exists(path):
                unreal.log_warning(f"[Wildreach] forest ground: {path} missing")
                continue
            t = unreal.AssetImportTask()
            t.filename = path
            t.destination_path = FOLDER
            t.destination_name = f"T_Forest{short}_{kind}"
            t.replace_existing = True
            t.automated = True
            t.save = False
            tasks.append(t)
    asset_tools.import_asset_tasks(tasks)
    for short in SETS:
        for kind in ("BCH", "N"):
            p = f"{FOLDER}/T_Forest{short}_{kind}"
            if not eal.does_asset_exist(p):
                continue
            tex = unreal.load_asset(p)
            if kind == "N":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
            else:
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
                tex.set_editor_property("srgb", True)
            tex.set_editor_property("max_texture_size", 1024)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD if kind == "BCH" else unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
            eal.save_loaded_asset(tex)
            unreal.log(f"[Wildreach] forest ground: imported {p}")


def sample(path, uv):
    tex = unreal.load_asset(path)
    cs = tex.get_editor_property("compression_settings")
    srgb = tex.get_editor_property("srgb")
    if cs == unreal.TextureCompressionSettings.TC_NORMALMAP:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    elif cs == unreal.TextureCompressionSettings.TC_MASKS:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    else:
        st = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    e = expr(unreal.MaterialExpressionTextureSample, texture=tex, sampler_type=st,
             sampler_source=unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    link(uv, e, "UVs")
    return e


def pin(src, out):
    e = expr(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    link(src, e, "", out)
    return e


def dot3(a, w):
    d = expr(unreal.MaterialExpressionDotProduct)
    link(a, d, "A")
    link(const3(w), d, "B")
    return d


def append2(a, b):
    return op(unreal.MaterialExpressionAppendVector, a, b)


def mx(a, b):
    return op(unreal.MaterialExpressionMax, a, b)


def build_material(path="/Game/Terrain/M_ForestGround", strong_tint=False):
    begin(path)
    wp = expr(unreal.MaterialExpressionWorldPosition)
    wx, wy, wz = mask(wp, "r"), mask(wp, "g"), mask(wp, "b")
    xy = append2(wx, wy)
    n = expr(unreal.MaterialExpressionVertexNormalWS)
    nz = mask(n, "b")
    vc = expr(unreal.MaterialExpressionVertexColor)
    rgb = mask(vc, "r", "g", "b")

    # ---- the six layers' weights from the land's UV channels
    t1 = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=1)
    t2 = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=2)
    t3 = expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=3)
    weights = [mask(t1, "r"), mask(t1, "g"), mask(t2, "r"), mask(t2, "g"), mask(t3, "r"), mask(t3, "g")]
    stones_raw = weights[4]

    # ---- each layer's colour, normal and height (cm tiles: the scans at 2.5 m as Trails tiles them; the pack's 3-4 m)
    def scan(short, tile):
        uv = mul(xy, k=1.0 / tile)
        c = sample(f"{FOLDER}/T_Forest{short}_BCH", uv)
        nn = sample(f"{FOLDER}/T_Forest{short}_N", uv)
        return mask(c, "r", "g", "b"), pin(c, "A"), nn

    def pack(name, tile):
        uv = mul(xy, k=1.0 / tile)
        c = sample(MW + name + "_col", uv)
        nn = sample(MW + name + "_nrm", uv)
        col_rgb = mask(c, "r", "g", "b")
        return col_rgb, dot3(col_rgb, (0.299, 0.587, 0.114)), nn

    litter_c, litter_h, litter_n = scan("Litter", 250.0)
    # the litter again at 6.1 m, mixed in by a slow wash, so its 2.5 m repeat never lines up
    litter_far = sample(f"{FOLDER}/T_ForestLitter_BCH", mul(xy, k=1.0 / 610.0))
    wash = noise(mul(xy, k=1.0 / 2300.0))
    litter_c = lerp(litter_c, mask(litter_far, "r", "g", "b"), mul(wash, k=0.45))
    moss_c, moss_h, moss_n = scan("Moss", 250.0)
    soil_c, soil_h, soil_n = scan("Soil", 250.0)
    grass_c, grass_h, grass_n = pack("Grass", 400.0)
    if strong_tint:
        # (round 8) the grass again at 9.7 m, turned a quarter, mixed in by a slow wash: its 4 m repeat ran across every
        # lawn as mowing stripes
        uv_g2 = mul(append2(wy, mul(wx, k=-1.0)), k=1.0 / 970.0)
        g2c, g2n = sample(MW + "Grass_col", uv_g2), sample(MW + "Grass_nrm", uv_g2)
        # and a third at 6.3 m turned 60 degrees: the three in shifting shares, so the grass's dirt flecks average out and
        # no one repeat lines up into bands (two still striped every lawn)
        uv_g3 = mul(append2(add(mul(wx, k=0.5), mul(wy, k=0.866)), add(mul(wx, k=-0.866), mul(wy, k=0.5))), k=1.0 / 630.0)
        g3c, g3n = sample(MW + "Grass_col", uv_g3), sample(MW + "Grass_nrm", uv_g3)
        w1 = sat(add(mul(noise(mul(xy, k=1.0 / 1700.0)), k=0.25), k=0.34))
        w2 = sat(add(mul(noise(add(mul(xy, k=1.0 / 1300.0), const(7.3))), k=0.25), k=0.33))
        w3 = sat(sub(sub(const(1.0), w1), w2))
        wsum = add(add(w1, w2), add(w3, const(0.001)))
        grass_c = op(unreal.MaterialExpressionDivide, add(add(mul(grass_c, w1), mul(mask(g2c, "r", "g", "b"), w2)), mul(mask(g3c, "r", "g", "b"), w3)), wsum)
        grass_n = op(unreal.MaterialExpressionDivide, add(add(mul(grass_n, w1), mul(g2n, w2)), mul(g3n, w3)), wsum)
        grass_h = dot3(grass_c, (0.299, 0.587, 0.114))
    stones_c, stones_h, stones_n = pack("Stones", 300.0)
    sand_c, sand_h, sand_n = pack("SandA", 400.0)
    cols = [litter_c, moss_c, soil_c, grass_c, stones_c, sand_c]
    heights = [litter_h, moss_h, soil_h, grass_h, stones_h, sand_h]
    norms = [litter_n, moss_n, soil_n, grass_n, stones_n, sand_n]
    roughs = [0.88, 0.95, 0.9, 0.86, 0.72, 0.9]

    # ---- each layer's weight wobbled by noise of its own (11-22: the weights change evenly across each 4 m square of the
    # land, so the borders between grounds ran straight along the squares; now they wander, a metre or two either way)
    var_a = sample("/Game/MWLandscapeAutoMaterial/Textures/Extra/TEX_MWAM_Variation_msk", mul(xy, k=1.0 / 900.0))
    var_b = sample("/Game/MWLandscapeAutoMaterial/Textures/Extra/TEX_MWAM_Variation_msk", mul(xy, k=1.0 / 260.0))
    wob = [mask(var_a, "r"), mask(var_a, "g"), mask(var_a, "b"), mask(var_b, "r"), mask(var_b, "g"), mask(var_b, "b")]
    weights = [mul(w, add(mul(wob[i], k=0.9), k=0.55)) for i, w in enumerate(weights)]
    # ---- height blend: a layer's weight raised by its own height; within 0.22 of the strongest it shows, else not
    ha = [mul(add(h, k=1.0), w) for h, w in zip(heights, weights)]
    m = ha[0]
    for x in ha[1:]:
        m = mx(m, x)
    floor_v = add(m, k=-0.22)
    b = [op(unreal.MaterialExpressionMax, sub(x, floor_v), None, const_b=0.0) for x in ha]
    total = b[0]
    for x in b[1:]:
        total = add(total, x)
    total = op(unreal.MaterialExpressionMax, total, None, const_b=0.0001)
    f = [op(unreal.MaterialExpressionDivide, x, total) for x in b]
    albedo = mul(cols[0], f[0])
    normal = mul(norms[0], f[0])
    rough = mul(f[0], k=roughs[0])
    for i in range(1, 6):
        albedo = add(albedo, mul(cols[i], f[i]))
        normal = add(normal, mul(norms[i], f[i]))
        rough = add(rough, mul(f[i], k=roughs[i]))

    # ---- snow and ice (the island's bright ground: its land colour over ~0.4) wear the snow's own texture and wind
    # ripples (11-22: they were the sand's, tinted white, and read as flat white with grey smudges)
    if strong_tint:
        snow_c, snow_h, snow_n = pack("Snow", 350.0)
        # and again at 11.3 m, turned (its 3.5 m repeat showed as a cell pattern across the snowfields from any height)
        uv_s2 = mul(append2(wy, mul(wx, k=-1.0)), k=1.0 / 1130.0)
        snow_c2, snow_n2 = sample(MW + "Snow_col", uv_s2), sample(MW + "Snow_nrm", uv_s2)
        snow_mix = add(mul(noise(mul(xy, k=1.0 / 1700.0)), k=0.5), k=0.25)
        snow_c = lerp(snow_c, mask(snow_c2, "r", "g", "b"), snow_mix)
        snow_n = lerp(snow_n, snow_n2, snow_mix)
        snowy = sat(mul(add(dot3(rgb, (0.299, 0.587, 0.114)), k=-0.4), k=4.0))
        albedo = lerp(albedo, snow_c, snowy)
        normal = lerp(normal, snow_n, snowy)
        rough = lerp(rough, const(0.72), snowy)
    # ---- steep ground is rock, projected from the side it faces (33-47 degrees: cos 0.84 .. 0.68)
    rock_w = sat(mul(add(nz, k=-0.84), k=-1.0 / 0.16))
    ax = unary(unreal.MaterialExpressionAbs, mask(n, "r"))
    ay = unary(unreal.MaterialExpressionAbs, mask(n, "g"))
    side = op(unreal.MaterialExpressionDivide, ax, add(add(ax, ay), k=0.0001))
    rock_x = sample(MW + "Rock_col", mul(append2(wy, wz), k=1.0 / 420.0))
    rock_y = sample(MW + "Rock_col", mul(append2(wx, wz), k=1.0 / 420.0))
    rock_c = lerp(mask(rock_y, "r", "g", "b"), mask(rock_x, "r", "g", "b"), side)
    rock_n = sample(MW + "Rock_nrm", mul(append2(wx, wz), k=1.0 / 420.0))
    if strong_tint:
        # (round 7) steep ice stays ice: where the land's colour is bright (the ice shelves' and the glacier's faces,
        # WRLand slopeRock.ice) the snow and ice texture streaks down the face with half the rock's relief; drawn as rock,
        # a glacier's knob read as grey stone with the ice faces stuck on it. Steep snowfields keep their rock showing.
        ice_keep = sub(const(1.0), mul(snowy, k=0.85))
        albedo = lerp(albedo, rock_c, mul(rock_w, ice_keep))
        normal = lerp(normal, rock_n, mul(rock_w, sub(const(1.0), mul(snowy, k=0.5))))
        rough = lerp(rough, const(0.86), mul(rock_w, ice_keep))
    else:
        albedo = lerp(albedo, rock_c, rock_w)
        normal = lerp(normal, rock_n, rock_w)
        rough = lerp(rough, const(0.86), rock_w)

    # ---- contacts: within 70 cm of any mesh (a boulder, a root, a log, a trunk) the ground goes mossy and damp
    dist = expr(unreal.MaterialExpressionDistanceToNearestSurface)
    contact = sat(unary(unreal.MaterialExpressionOneMinus, mul(dist, k=1.0 / 70.0)))
    contact = mul(contact, contact)
    damp_moss = mul(moss_c, k=0.62)
    albedo = lerp(albedo, damp_moss, mul(contact, k=0.7))
    rough = lerp(rough, const(0.97), mul(contact, k=0.5))

    # ---- the sub-zone's palette as a tint (its hue at unit brightness), and a slow brightness wash (31 m)
    lum = add(dot3(rgb, (0.299, 0.587, 0.114)), k=0.03)
    hue = op(unreal.MaterialExpressionDivide, rgb, lum)
    tint = lerp(const3((1.0, 1.0, 1.0)), mul(hue, k=0.95), const(0.22))
    if strong_tint:
        # The island: the vertex colour is the land's colour; the textures give only their light and shade (their
        # brightness, about 0.25-0.4, scaled to about 0.65-1).
        shade = sat(mul(dot3(albedo, (0.299, 0.587, 0.114)), k=2.6))
        # Bright ground (snow, glacier ice: linear brightness over ~0.35) keeps at least 0.55 of its own colour, so the dark
        # steep-rock texture under an ice cliff can't turn the cliff into a dark trench (at 0.8, 11-22, the cliffs read as
        # flat white walls with no rock in them).
        bright = sat(mul(add(dot3(rgb, (0.299, 0.587, 0.114)), k=-0.35), k=3.0))
        shade = op(unreal.MaterialExpressionMax, shade, mul(bright, k=0.55))
        amount = expr(unreal.MaterialExpressionScalarParameter, parameter_name="TintAmount", default_value=0.8)
        # Dark ground (basalt, obsidian, black sand: linear brightness under ~0.12) takes nearly all its colour from the
        # land, so a pale texture's share can't lift it to grey; brighter ground keeps TintAmount as it was.
        dark = sub(const(1.0), sat(mul(dot3(rgb, (0.299, 0.587, 0.114)), k=8.0)))
        amount = add(amount, mul(sub(const(1.0), amount), dark))
        albedo = lerp(mul(albedo, tint), mul(rgb, shade), amount)
        # (snow a shade under paper white: at full strength it blew out to flat white beside the meadow)
        albedo = mul(albedo, lerp(const(1.0), const(0.84), snowy))
    else:
        albedo = mul(albedo, tint)

    # ---- the Volcanic's bare lava (11-22: the lava fields and obsidian ridges read as flat black gravel). Dark stony land
    # (pahoehoe, aa, basalt, lava crust, obsidian: linear brightness under ~0.09, mostly the stones layer) wears the LavaMat
    # pack's flows: pahoehoe's ropy folds (two flows, their folds crossways to each other) and aa's clinker, in patches tens
    # of metres across; the land's colour lit by their relief, the cave rock's grain over it, broad lobes at 60 m. Glassy
    # pahoehoe crests; the land's own smoothness (vertex alpha = 1 - roughness: obsidian) where it is glossier.
    if strong_tint:
        LV = "/Game/LavaMat/Textures/T_LavaMat"
        lum_v = dot3(rgb, (0.299, 0.587, 0.114))
        dark_v = sub(const(1.0), sat(mul(add(lum_v, k=-0.085), k=40.0)))
        lava_w = mul(mul(dark_v, sat(mul(add(stones_raw, k=-0.55), k=4.0))), sub(const(1.0), rock_w))
        aa = sat(mul(add(noise(mul(xy, k=1.0 / 4100.0)), k=-0.38), k=4.0))   # (more clinker: the smooth flows read as sand)
        turn = sat(mul(add(noise(mul(xy, k=1.0 / 2700.0)), k=-0.5), k=6.0))
        uv_p = mul(xy, k=1.0 / 330.0)
        uv_q = mul(xy, k=1.0 / 420.0)
        uv_a = mul(xy, k=1.0 / 260.0)
        h_p = lerp(mask(tex(LV + "7_height", uv_p), "r"), mask(tex(LV + "6_height", uv_q), "r"), turn)
        n_p = lerp(mask(tex(LV + "7_normal", uv_p), "r", "g", "b"), mask(tex(LV + "6_normal", uv_q), "r", "g", "b"), turn)
        h_lava = lerp(h_p, mask(tex(LV + "10_height", uv_a), "r"), aa)
        n_lava = lerp(n_p, mask(tex(LV + "10_normal", uv_a), "r", "g", "b"), aa)
        grain = dot3(mask(tex(MW + "Rock_col", mul(xy, k=1.0 / 120.0)), "r", "g", "b"), (0.299, 0.587, 0.114))
        lobes = mask(tex(LV + "1_height", mul(xy, k=1.0 / 6000.0)), "r")
        lit = mul(mul(lerp(const(0.6), const(1.4), h_lava), lerp(const(0.55), const(1.35), sat(mul(grain, k=2.6)))),
                  lerp(const(0.85), const(1.12), lobes))
        albedo = lerp(albedo, mul(rgb, lit), lava_w)
        normal = lerp(normal, n_lava, lava_w)
        land_r = sub(const(1.0), mask(vc, "a"))
        lava_r = lerp(lerp(const(0.82), const(0.5), h_lava), const(0.93), aa)
        lava_r = op(unreal.MaterialExpressionMin, lava_r, add(land_r, k=0.1))
        rough = lerp(rough, lava_r, lava_w)
    if strong_tint:
        # (the island: two sizes, half as strong; at 0.34 on one 31 m size the meadow read as camouflage from the air)
        far = add(mul(noise(mul(xy, k=1.0 / 5200.0)), k=0.6), mul(noise(mul(xy, k=1.0 / 1900.0)), k=0.4))
        albedo = mul(albedo, add(mul(far, k=0.18), k=0.91))
    else:
        far = noise(mul(xy, k=1.0 / 3100.0))
        albedo = mul(albedo, add(mul(far, k=0.34), k=0.83))

    # ---- rain (the land's Wet): darker and glossier on what faces up
    wet = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Wet", default_value=0.0)
    wet_up = mul(wet, sat(add(mul(nz, k=1.4), k=-0.25)))
    if strong_tint:
        # The shore's wet band (round 7): the island's first UV's X is how wet the shore is there (WRLand HfWet: from
        # where the sea's edge is drawn up the beach, and round the lake); it darkens and shines as rain does. (In the
        # vertex colour alone it barely showed: this ground takes its brightness from its textures.)
        shore = sat(mask(expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0), "r"))
        wet_up = op(unreal.MaterialExpressionMax, wet_up, mul(shore, k=0.85))
    albedo = lerp(albedo, mul(albedo, k=0.6), wet_up)
    rough = lerp(rough, const(0.24), mul(wet_up, k=0.85))

    mel.connect_material_property(albedo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(const(0.4), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")


args = [a for a in sys.argv[1:] if not a.startswith("-")]
scans = args[0] if args else os.environ.get("WR_FOREST_SCANS", "")
if scans and os.path.isdir(scans):
    import_scans(scans)
else:
    unreal.log("[Wildreach] forest ground: no scans dir given; using the textures already in /Game/Terrain/Forest")
if "--island" in sys.argv[1:]:
    build_material("/Game/Terrain/M_IslandGround", True)
else:
    build_material()
