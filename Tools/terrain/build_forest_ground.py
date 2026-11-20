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
    stones_c, stones_h, stones_n = pack("Stones", 300.0)
    sand_c, sand_h, sand_n = pack("SandA", 400.0)
    cols = [litter_c, moss_c, soil_c, grass_c, stones_c, sand_c]
    heights = [litter_h, moss_h, soil_h, grass_h, stones_h, sand_h]
    norms = [litter_n, moss_n, soil_n, grass_n, stones_n, sand_n]
    roughs = [0.88, 0.95, 0.9, 0.86, 0.72, 0.9]

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

    # ---- steep ground is rock, projected from the side it faces (33-47 degrees: cos 0.84 .. 0.68)
    rock_w = sat(mul(add(nz, k=-0.84), k=-1.0 / 0.16))
    ax = unary(unreal.MaterialExpressionAbs, mask(n, "r"))
    ay = unary(unreal.MaterialExpressionAbs, mask(n, "g"))
    side = op(unreal.MaterialExpressionDivide, ax, add(add(ax, ay), k=0.0001))
    rock_x = sample(MW + "Rock_col", mul(append2(wy, wz), k=1.0 / 420.0))
    rock_y = sample(MW + "Rock_col", mul(append2(wx, wz), k=1.0 / 420.0))
    rock_c = lerp(mask(rock_y, "r", "g", "b"), mask(rock_x, "r", "g", "b"), side)
    rock_n = sample(MW + "Rock_nrm", mul(append2(wx, wz), k=1.0 / 420.0))
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
        amount = expr(unreal.MaterialExpressionScalarParameter, parameter_name="TintAmount", default_value=0.8)
        # Dark ground (basalt, obsidian, black sand: linear brightness under ~0.12) takes nearly all its colour from the
        # land, so a pale texture's share can't lift it to grey; brighter ground keeps TintAmount as it was.
        dark = sub(const(1.0), sat(mul(dot3(rgb, (0.299, 0.587, 0.114)), k=8.0)))
        amount = add(amount, mul(sub(const(1.0), amount), dark))
        albedo = lerp(mul(albedo, tint), mul(rgb, shade), amount)
    else:
        albedo = mul(albedo, tint)
    far = noise(mul(xy, k=1.0 / 3100.0))
    albedo = mul(albedo, add(mul(far, k=0.34), k=0.83))

    # ---- rain (the land's Wet): darker and glossier on what faces up
    wet = expr(unreal.MaterialExpressionScalarParameter, parameter_name="Wet", default_value=0.0)
    wet_up = mul(wet, sat(add(mul(nz, k=1.4), k=-0.25)))
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
