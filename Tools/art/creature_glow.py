"""Glow for creatures whose texture has fire or ice in it (lava cracks, ember horns, magma spikes, ice antlers), so it
really shines instead of being painted on. Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/art/creature_glow.py"

  /Game/Creatures/M_CreatureGlow  master: the creature's own texture, matte (fur and hide); wherever that texture leans
                                  far enough toward the glow's hue (fire: red over blue; frost: blue over red) and is
                                  bright enough, it glows, pulsing slowly, each creature at its own phase.
  MI_<Name>Glow                   one per creature below, in its folder; creatures.json "materials" points at it.
Only creatures whose glowing parts stand apart from the rest are listed: orange or blue fur all over would glow all over.
The master also takes a normal map: Tools/art/bake_detail.py bakes scales / feathers / fur into
C:/Dev/WildreachArt/out/<id>_detail_color.png and _normal.png; a creature that has them gets them imported as
<Name>_DetailColor / <Name>_DetailNormal and used in place of its plain texture. Creatures here with no glow (strength
0) get MI_<Name>Skin instead, for that detail alone.
"""
import os
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MASTER = "/Game/Creatures/M_CreatureGlow"
FIRE = dict(hue=(1, 0, -1), threshold=0.3, gate=(1, 0, 0), gate_level=0.45, tint=(1, 1, 1), speed=1.7)
FROST = dict(hue=(-1, 0, 1), threshold=0.22, gate=(0, 0, 1), gate_level=0.7, tint=(0.6, 0.85, 1.0), speed=0.9)
# the Glowmoth's cyan eyespots: bluer and brighter than FROST asks, so its pale blue fur stays dark
MOTH = dict(hue=(-1, 0, 1), threshold=0.35, gate=(0, 0, 1), gate_level=0.55, tint=(0.7, 0.9, 1.0), speed=0.8)
# eyes painted clear ice blue: far bluer than any fur, so a blue mane next to them stays dark
EYES = dict(hue=(-1, 0, 1), threshold=0.72, gate=(0, 0, 1), gate_level=0.75, tint=(0.6, 0.9, 1.0), speed=0.6)
CREATURES = {
    "Magmaw": (FIRE, 6.0),       # magma spikes along the back
    "Cinderfang": (FIRE, 8.0),   # ember cracks
    "Basaltback": (FIRE, 5.0),   # glowing vents on the shell
    "Emberhorn": (FIRE, 6.0),    # ember slashes and the horn
    "Kilnmite": (FIRE, 6.0),     # kiln cracks across the copper shell (the copper itself stays under the gate)
    "Glowmoth": (MOTH, 5.0),     # the cyan eyespots on its wings (gm_fix.py made them clear cyan)
    "Pumicehog": (FIRE, 4.0),    # the cracks through its orange back (ph_fix.py keeps the orange under the gate)
    "Rimeclaw": (EYES, 6.0),     # its ice-blue eyes (rc_fix.py paints them and keeps the blue mane under the threshold)
    "Snowpuff": (FIRE, 0.0),     # no glow: baked feathers
    "Aurora_fox": (FIRE, 0.0),   # no glow: baked fur and a softened aurora
    "Glacimaw": (FIRE, 0.0, {"Roughness": 0.6, "Specular": 0.35}),  # no glow: a soft sheen (glossier, its body took the sky's white)
    "Obsidian_warden": (FIRE, 7.0),  # the orange cracks in its black armour (they were only painted and barely showed at night)
}
# A third entry sets other scalars on the creature's material (the master's Roughness and Specular).
DETAIL_DIR = "C:/Dev/WildreachArt/out"


def get_or_create(path, cls, factory):
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit("/", 1)
    return tools.create_asset(name, folder, cls, factory)


def build_master(default_tex):
    mat = get_or_create(MASTER, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    n = [0]

    def expr(cls, **props):
        n[0] += 1
        e = mel.create_material_expression(mat, cls, -1400 + (n[0] % 7) * 200, (n[0] // 7) * 160)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def scalar(name, v):
        return expr(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)

    def rgb(node):
        m = expr(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
        link(node, m)
        return m

    def vector(name, v):
        return rgb(expr(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*v, 1.0)))

    def const(v):
        return expr(unreal.MaterialExpressionConstant, r=v)

    def link(a, b, pin="", out=""):
        mel.connect_material_expressions(a, out, b, pin)

    def op(cls, a, b):
        e = expr(cls)
        link(a, e, "A")
        link(b, e, "B")
        return e

    def sat(node):
        e = expr(unreal.MaterialExpressionSaturate)
        link(node, e)
        return e

    tex = expr(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="BaseColorTex",
               texture=unreal.load_asset(default_tex), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    col = rgb(tex)
    # hue = saturate((dot(col, GlowHue) - GlowThreshold) * GlowSharpness): how far the colour leans toward the glow
    lean = op(unreal.MaterialExpressionSubtract, op(unreal.MaterialExpressionDotProduct, col, vector("GlowHue", FIRE["hue"])),
              scalar("GlowThreshold", FIRE["threshold"]))
    hue = sat(op(unreal.MaterialExpressionMultiply, lean, scalar("GlowSharpness", 4.0)))
    # gate = saturate((dot(col, GlowGate) - GlowGateLevel) * 4): and bright in that channel (not a dull brown or navy)
    lit = op(unreal.MaterialExpressionSubtract, op(unreal.MaterialExpressionDotProduct, col, vector("GlowGate", FIRE["gate"])),
             scalar("GlowGateLevel", FIRE["gate_level"]))
    gate = sat(op(unreal.MaterialExpressionMultiply, lit, const(4.0)))
    mask = op(unreal.MaterialExpressionMultiply, hue, gate)
    # pulse = 1 - PulseDepth * (0.5 + 0.5 sin(time * PulseSpeed + phase)), phase from where the creature stands
    t = op(unreal.MaterialExpressionMultiply, expr(unreal.MaterialExpressionTime), scalar("PulseSpeed", FIRE["speed"]))
    px = expr(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    link(expr(unreal.MaterialExpressionObjectPositionWS), px)
    s = expr(unreal.MaterialExpressionSine)
    link(op(unreal.MaterialExpressionAdd, t, op(unreal.MaterialExpressionMultiply, px, const(0.013))), s)
    wave = op(unreal.MaterialExpressionAdd, op(unreal.MaterialExpressionMultiply, s, const(0.5)), const(0.5))
    pulse = op(unreal.MaterialExpressionSubtract, const(1.0), op(unreal.MaterialExpressionMultiply, wave, scalar("PulseDepth", 0.35)))
    glow = op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, col, mask),
              op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, pulse, scalar("GlowStrength", 6.0)),
                 vector("GlowTint", FIRE["tint"])))
    normal = expr(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name="NormalTex",
                  texture=unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(tex, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(scalar("Roughness", 0.82), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Specular", 0.3), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def texture_of(name):
    """The creature's colour texture: <id>_Image_0, or whatever *_Image_0 its import named it (after the source file)."""
    path = f"/Game/Creatures/{name}/{name.lower()}_Image_0"
    if eal.does_asset_exist(path):
        return path
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in reg.get_assets_by_path(f"/Game/Creatures/{name}"):
        if str(a.asset_name).endswith("_Image_0") and str(a.asset_class_path.asset_name) == "Texture2D":
            return f"/Game/Creatures/{name}/{a.asset_name}"
    return None


def import_detail(name):
    """Bake outputs for this creature, imported (again) into its folder: (colour path, normal path) or (None, None)."""
    pngs = [f"{DETAIL_DIR}/{name.lower()}_detail_{k}.png" for k in ("color", "normal")]
    if not all(os.path.exists(p) for p in pngs):
        return None, None
    paths = []
    for png, asset in zip(pngs, (f"{name}_DetailColor", f"{name}_DetailNormal")):
        task = unreal.AssetImportTask()
        task.filename = png
        task.destination_path = f"/Game/Creatures/{name}"
        task.destination_name = asset
        task.automated = True
        task.replace_existing = True
        task.save = True
        tools.import_asset_tasks([task])
        path = f"/Game/Creatures/{name}/{asset}"
        if asset.endswith("Normal"):
            t = unreal.load_asset(path)
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False)
            t.set_editor_property("flip_green_channel", True)  # Blender bakes OpenGL normals
            eal.save_loaded_asset(t)
        paths.append(path)
    return paths[0], paths[1]


master = build_master(texture_of("Cinderfang") or "/Engine/EngineResources/DefaultTexture")
for name, (kind, strength, *more) in CREATURES.items():
    tex = texture_of(name)
    if not tex:
        print(f"[Wildreach] no texture for {name}")
        continue
    detail_col, detail_nrm = import_detail(name)
    mi_name = f"MI_{name}Glow" if strength > 0 else f"MI_{name}Skin"
    mi = get_or_create(f"/Game/Creatures/{name}/{mi_name}", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", unreal.load_asset(detail_col or tex))
    if detail_nrm:
        mel.set_material_instance_texture_parameter_value(mi, "NormalTex", unreal.load_asset(detail_nrm))
    for p, v in (("GlowHue", kind["hue"]), ("GlowGate", kind["gate"]), ("GlowTint", kind["tint"])):
        mel.set_material_instance_vector_parameter_value(mi, p, unreal.LinearColor(*v, 1.0))
    for p, v in (("GlowThreshold", kind["threshold"]), ("GlowGateLevel", kind["gate_level"]), ("PulseSpeed", kind["speed"]),
                 ("GlowStrength", strength), *(more[0].items() if more else ())):
        mel.set_material_instance_scalar_parameter_value(mi, p, v)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    print(f"[Wildreach] {mi_name}" + (" with baked detail" if detail_nrm else ""))
