"""Wools for the townsfolk (11-22, round 9): two dark ones for the hose: the town clothes' leggings are skin-tight shells, and in the outfit
set's linen or leather (tan) they read as bare legs. MI_Wool_Dark (charcoal) and MI_Wool_Brown on M_WR_Garment's
linen weave; the dyed ones (round 9) for their shirts and bodices. Safe to run again (it remakes them).

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
    # Round 9: dyed wools for the shirts and bodices. In the undyed linen (tan) a skin-tight shirt read as bare skin: the
    # hall's steward looked shirtless. Woad blue, weld green, madder russet, and an oatmeal far paler than skin.
    "MI_Wool_Blue": ((0.075, 0.105, 0.17), (0.03, 0.026, 0.022)),
    "MI_Wool_Green": ((0.105, 0.13, 0.065), (0.035, 0.028, 0.02)),
    "MI_Wool_Russet": ((0.30, 0.095, 0.05), (0.04, 0.03, 0.022)),
    "MI_Wool_Oat": ((0.56, 0.54, 0.48), (0.06, 0.045, 0.03)),
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
