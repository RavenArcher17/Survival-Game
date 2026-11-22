"""Two dark wools for the townsfolk's hose (11-22): the town clothes' leggings are skin-tight shells, and in the outfit
set's linen or leather (tan) they read as bare legs. MI_Wool_Dark (charcoal) and MI_Wool_Brown on M_WR_Garment's
linen weave. Safe to run again (it remakes them).

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
DIR = "/Game/Wildreach/Characters/Hero/Outfits/Common"
parent = unreal.load_asset(DIR + "/M_WR_Garment")
linen = unreal.load_asset(DIR + "/T_Linen_BC")
linen_n = unreal.load_asset(DIR + "/T_Linen_N")
WOOLS = {
    # (tint, trim): the leggings' vertices are trim (vertex colour R = 1), drawn TrimColor x the weave x TrimTexBoost 2.2
    "MI_Wool_Dark": ((0.20, 0.19, 0.18), (0.045, 0.042, 0.04)),
    "MI_Wool_Brown": ((0.32, 0.22, 0.15), (0.075, 0.05, 0.032)),
}
made = []
for name, (tint, trim) in WOOLS.items():
    path = f"{DIR}/{name}"
    # a copy of MI_Linen_Dark (its trim colour and every other setting; a fresh instance took the garment's default fire
    # orange trim and drew orange hose), retinted
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    mi = eal.duplicate_asset(DIR + "/MI_Linen_Dark", path)
    mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
    mel.set_material_instance_vector_parameter_value(mi, "TrimColor", unreal.LinearColor(*trim, 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.9)
    mel.set_material_instance_scalar_parameter_value(mi, "Tile", 4.0)
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    made.append(name)
unreal.log("[Wildreach] wools: " + ", ".join(made))
