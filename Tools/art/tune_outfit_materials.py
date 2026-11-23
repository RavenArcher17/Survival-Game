"""Per-material corrections to the armour sets' look (round 11 look pass, Raven: "keep at it until they look
perfect/correct"), each checked in pictures of the bot wearing the set. Safe to run again (it sets values).

  MI_Forge_Plate   molten-cracked obsidian: the cracks at 1.2 tiles and 3 nits glowed everywhere, small and even, a
                   leopard print; now fewer, larger plates (0.45 tiles) and a dimmer glow in the seams
  MI_Rime_Fur      frost fur: the brown fur texture x4 drew a tan ring like a bread roll round the neck; desaturated,
                   pale grey-white, its fur relief stronger
  MI_Dune_Gold     (the Dunewrap's collar and fittings) the same tan ring: worked gold, metallic, darker
  MI_Dune_White    its sash and headcloth drew flat paper-white: an unbleached linen, its weave showing
Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
ROOT = "/Game/Wildreach/Characters/Hero/Outfits"
CHANGES = {
    "MI_Forge_Plate": {"Tile": 0.45, "SeamNits": 1.1, "Roughness": 0.4},
    "MI_Rime_Fur": {"Desaturate": 0.92, "Tint": (2.6, 2.7, 2.9), "NormalStrength": 1.6, "Tile": 3.5},
    "MI_Dune_Gold": {"Metallic": 0.9, "Roughness": 0.35, "Desaturate": 0.0},
    "MI_Dune_White": {"Tint": (0.86, 0.8, 0.68), "NormalStrength": 1.4, "Roughness": 0.85},
}
ar = unreal.AssetRegistryHelpers.get_asset_registry()
found = {str(a.asset_name): a for a in ar.get_assets_by_path(ROOT, recursive=True) if str(a.asset_name) in CHANGES}
log = []
for name, params in CHANGES.items():
    if name not in found:
        log.append("%s: not found" % name)
        continue
    mi = found[name].get_asset()
    before = {}
    for k, v in params.items():
        if isinstance(v, tuple):
            c = mel.get_material_instance_vector_parameter_value(mi, k)
            before[k] = (round(c.r, 2), round(c.g, 2), round(c.b, 2))
            mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1.0))
        else:
            before[k] = round(mel.get_material_instance_scalar_parameter_value(mi, k), 2)
            mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    log.append("%s: %s -> %s" % (name, before, params))
with open(unreal.Paths.project_saved_dir() + "tune_outfit_materials.txt", "w") as f:
    f.write("\n".join(log) + "\n")
