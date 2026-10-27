"""Effects assets for WRFx (the C++ sprite particles): the sprite atlas and the two particle materials. Safe to re-run.
Run headless (after `node Tools/fx/make_fx_atlas.mjs`):
  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/fx/make_fx_assets.py"

  /Game/FX/T_FxAtlas  the 4 x 4 sprite atlas (linear, no sRGB: its grey levels are intensities and masks)
  /Game/FX/M_FxAdd    additive, unlit glow: fire, sparks, magic, embers. Emissive = atlas rgb * a * vertex colour * a * UV1.x
  /Game/FX/M_FxLit    translucent, lit by the scene (volumetric, so the sun and the night reach it): smoke, dust, chips,
                      blood, snow, ash, and flames and lava that must stay saturated over bright sand. Base = atlas rgb
                      * vertex colour; opacity = atlas a * vertex alpha; emissive = base * UV1.x.

The particle renderer (WRFx.cpp) feeds per-sprite colour through the vertex colour and the HDR intensity through UV1.x.
Both fade softly where they meet geometry (depth fade over UV1.y cm), so puffs on the ground show no hard line. The additive one
partly undoes auto-exposure (EyeAdaptationInverse), so a fire bolt is bright at noon without blinding at midnight.
Only engine nodes are used, so a fresh clone builds them.
"""
import os
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER = "/Game/FX"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
ATLAS_PNG = os.path.join(PROJECT, "Saved", "FX", "fx_atlas.png")


def log(msg):
    unreal.log(f"[Wildreach] {msg}")


def import_atlas():
    if not os.path.exists(ATLAS_PNG):
        raise RuntimeError(f"{ATLAS_PNG} missing: run node Tools/fx/make_fx_atlas.mjs first")
    task = unreal.AssetImportTask()
    task.filename = ATLAS_PNG
    task.destination_path = FOLDER
    task.destination_name = "T_FxAtlas"
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(FOLDER + "/T_FxAtlas")
    tex.set_editor_property("srgb", False)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    eal.save_loaded_asset(tex)
    log(f"imported T_FxAtlas from {ATLAS_PNG}")
    return tex


class Graph:
    def __init__(self, path):
        if eal.does_asset_exist(path):
            self.mat = unreal.load_asset(path)
            mel.delete_all_material_expressions(self.mat)
        else:
            folder, name = path.rsplit("/", 1)
            self.mat = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
        self.n = 0

    def expr(self, cls, **props):
        self.n += 1
        e = mel.create_material_expression(self.mat, cls, -1400 + (self.n % 6) * 220, (self.n // 6) * 180)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, pin="", out=""):
        if not mel.connect_material_expressions(a, out, b, pin):
            raise RuntimeError(f"could not link {a.get_name()}.{out} -> {b.get_name()}.{pin}")

    def expr_link(self, cls, a, **props):
        e = self.expr(cls, **props)
        self.link(a, e)
        return e

    def mul(self, a, b, aout="", bout=""):
        e = self.expr(unreal.MaterialExpressionMultiply)
        self.link(a, e, "A", aout)
        self.link(b, e, "B", bout)
        return e

    def op_max(self, a, k):
        e = self.expr(unreal.MaterialExpressionMax, const_b=k)
        self.link(a, e, "A")
        return e

    def out(self, e, prop, pin=""):
        mel.connect_material_property(e, pin, prop)

    def finish(self):
        mel.recompile_material(self.mat)
        eal.save_loaded_asset(self.mat)
        log(f"built {self.mat.get_name()} ({self.n} expressions)")


def common_inputs(g, atlas):
    tex = g.expr(unreal.MaterialExpressionTextureSample, texture=atlas, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    uv0 = g.expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    g.link(uv0, tex, "UVs")
    vc = g.expr(unreal.MaterialExpressionVertexColor)
    uv1 = g.expr(unreal.MaterialExpressionTextureCoordinate, coordinate_index=1)
    glow = g.expr(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    g.link(uv1, glow)
    # UV1.y = how softly the sprite fades into geometry (cm): puffs fade over ~25, flat sprites on the ground ~3.
    soft = g.expr(unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
    g.link(uv1, soft)
    soft = g.op_max(soft, 1.0)
    return tex, vc, glow, soft


def depth_fade(g, soft):
    fade = g.expr(unreal.MaterialExpressionDepthFade, fade_distance_default=25.0)
    g.link(soft, fade, "FadeDistance")
    return fade


def build_add(atlas):
    g = Graph(FOLDER + "/M_FxAdd")
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    tex, vc, glow, soft = common_inputs(g, atlas)
    shape = g.mul(tex, tex, "RGB", "A")              # rgb * a
    tint = g.mul(vc, vc, "", "A")                    # vertex rgb * a
    e = g.mul(shape, tint)
    e = g.mul(e, glow)                               # HDR intensity
    # Partly undo auto-exposure: night exposure would otherwise blow a fire bolt out to white.
    try:
        inv = g.expr(unreal.MaterialExpressionEyeAdaptationInverse)
        lerp = g.expr(unreal.MaterialExpressionLinearInterpolate, const_a=1.0, const_alpha=0.6)
        g.link(inv, lerp, "B")
        e = g.mul(e, lerp)
    except Exception as ex:  # older engines: no exposure compensation
        log(f"M_FxAdd: no EyeAdaptationInverse ({ex})")
    fade = depth_fade(g, soft)
    e = g.mul(e, fade)
    g.out(e, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    g.finish()


def build_lit(atlas):
    g = Graph(FOLDER + "/M_FxLit")
    m = g.mat
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    tex, vc, glow, soft = common_inputs(g, atlas)
    base = g.mul(tex, vc, "RGB", "")
    # A sprite that glows (glow >= 1: flames, molten lava) isn't lit as well, or the sun washes it out to cream.
    unlit = g.expr(unreal.MaterialExpressionOneMinus)
    g.link(glow, unlit)
    unlit = g.expr_link(unreal.MaterialExpressionSaturate, unlit)
    g.out(g.mul(base, unlit), unreal.MaterialProperty.MP_BASE_COLOR)
    alpha = g.mul(tex, vc, "A", "A")
    fade = depth_fade(g, soft)
    g.link(alpha, fade, "Opacity")
    g.out(fade, unreal.MaterialProperty.MP_OPACITY)
    emissive = g.mul(base, glow)
    # Self-glow (lava, flames drawn as lit sprites) gets the same partial exposure compensation as M_FxAdd.
    try:
        inv = g.expr(unreal.MaterialExpressionEyeAdaptationInverse)
        lerp = g.expr(unreal.MaterialExpressionLinearInterpolate, const_a=1.0, const_alpha=0.6)
        g.link(inv, lerp, "B")
        emissive = g.mul(emissive, lerp)
    except Exception as ex:
        log(f"M_FxLit: no EyeAdaptationInverse ({ex})")
    g.out(emissive, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = g.expr(unreal.MaterialExpressionConstant, r=1.0)
    g.out(rough, unreal.MaterialProperty.MP_ROUGHNESS)
    spec = g.expr(unreal.MaterialExpressionConstant, r=0.0)
    g.out(spec, unreal.MaterialProperty.MP_SPECULAR)
    g.finish()


atlas = import_atlas()
build_add(atlas)
build_lit(atlas)
log("FX assets done")
