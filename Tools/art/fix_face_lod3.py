"""Gives every hero face's mid- and far-distance skin materials their textures (11-22). Safe to run again (it finds
nothing to do).

Five faces (F1, F2, F3, F6, M3) came in with a MI_Face_Skin_Baked_LOD3_VT that never set "Basecolor Baked VT",
"Normal Baked VT", "SRMF Baked VT" and "Scatter Baked VT": their head LOD3 textures were never baked. That level of
detail draws from about 4 to 12 m, where those faces fell back to the skin material's placeholder textures and went
black (Hearthmoot's townsfolk, and any survivor wearing those faces, seen across a room). This copies the four from the
same face's LOD1 material (MetaHuman head LODs share one UV layout); the LOD3 baked normal ("Normal LOD Baked") stays.
And every face's MI_Face_Skin_Baked_LOD5to7_VT (the far levels, and the townsfolk's) lacked "SRMF Baked VT": the skin
material (MI_WR_Skin) then read its specular / roughness / metal from the placeholder and the face went shiny black.

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
ROOT = "/Game/Wildreach/Characters/Hero/Unpacked/"
PARAMS = ["Basecolor Baked VT", "Normal Baked VT", "SRMF Baked VT", "Scatter Baked VT"]
fixed = []
for path in eal.list_assets(ROOT, recursive=True, include_folder=False):
    name = path.split(".")[0]
    if not (name.endswith("/Face/Materials/MI_Face_Skin_Baked_LOD3_VT") or name.endswith("/Face/Materials/MI_Face_Skin_Baked_LOD5to7_VT")):
        continue
    lod3 = unreal.load_asset(name)
    lod1 = unreal.load_asset(name.replace("LOD3_VT", "LOD1_VT").replace("LOD5to7_VT", "LOD1_VT"))
    if not lod3 or not lod1:
        continue
    have = {str(v.parameter_info.name) for v in lod3.get_editor_property("texture_parameter_values")}
    src = {str(v.parameter_info.name): v.parameter_value for v in lod1.get_editor_property("texture_parameter_values")}
    missing = [p for p in PARAMS if p not in have and src.get(p)]
    if not missing:
        continue
    for p in missing:
        mel.set_material_instance_texture_parameter_value(lod3, p, src[p])
    mel.update_material_instance(lod3)
    eal.save_loaded_asset(lod3)
    fixed.append(name.split("/Unpacked/")[1].split("/")[0] + " " + name.rsplit("_Baked_", 1)[1] + " (" + ", ".join(missing) + ")")
unreal.log("[Wildreach] face skin textures set: " + (("; ".join(fixed)) if fixed else "none needed"))
