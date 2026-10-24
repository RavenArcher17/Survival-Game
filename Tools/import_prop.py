# Import textured image-to-3D props (glb from Tools/art/build3d_batch.py) as static meshes into /Game/Props/<Name>.
# Run headless:  UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/import_prop.py"
#   WR_IMPORTS="C:\path\a_textured.glb=Name;C:\path\b_textured.glb=Other"   (empty: just re-fix existing props' materials)
# Nanite on (AI meshes are ~40k triangles and the dressing places thousands); no collision (the dressing adds its own).
# The glTF importer's materials derive from an engine material without the Nanite / instancing usage flags (the game would
# draw the default material), so each prop material is re-parented to /Game/Props/M_Prop, keeping its texture.
# Like import_creature.py, an existing folder is skipped: move it out of Content first to replace a prop.
import os
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
reg = unreal.AssetRegistryHelpers.get_asset_registry()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
PROP_MAT = "/Game/Props/M_Prop"


def prop_material():
    """Base colour texture + a little roughness, with the usage flags the dressing needs."""
    if eal.does_asset_exist(PROP_MAT):
        return unreal.load_asset(PROP_MAT)
    mat = tools.create_asset("M_Prop", "/Game/Props", unreal.Material, unreal.MaterialFactoryNew())
    tex = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    tex.set_editor_property("parameter_name", "BaseColorTexture")
    mel.connect_material_property(tex, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 250)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    for usage in (unreal.MaterialUsage.MATUSAGE_NANITE, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES):
        mel.set_base_material_usage(mat, usage)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat


def fix_materials(folder):
    parent = prop_material()
    for a in reg.get_assets_by_path(folder, recursive=True):
        mi = a.get_asset()
        if not isinstance(mi, unreal.MaterialInstanceConstant) or mi.get_editor_property("parent") == parent:
            continue
        texture = None
        for n in mel.get_texture_parameter_names(mi):
            t = mel.get_material_instance_texture_parameter_value(mi, n)
            if t and ("base" in str(n).lower() or "color" in str(n).lower() or "diffuse" in str(n).lower()):
                texture = t
        mel.set_material_instance_parent(mi, parent)
        if texture:
            mel.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", texture)
        eal.save_loaded_asset(mi)
        print(f"[Wildreach] {mi.get_name()} -> M_Prop (texture {texture.get_name() if texture else 'none'})")


for item in os.environ.get("WR_IMPORTS", "").split(";"):
    if not item.strip():
        continue
    path, name = (s.strip() for s in item.rsplit("=", 1))
    dest = f"/Game/Props/{name}"
    if reg.get_assets_by_path(dest, recursive=True):
        print(f"[Wildreach] SKIPPED {name}: {dest} already exists")
        continue
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.automated = True
    task.save = True
    task.replace_existing = True
    tools.import_asset_tasks([task])
    meshes = []
    for a in reg.get_assets_by_path(dest, recursive=True):
        asset = a.get_asset()
        if isinstance(asset, unreal.StaticMesh):
            settings = asset.get_editor_property("nanite_settings")
            settings.enabled = True
            asset.set_editor_property("nanite_settings", settings)
            unreal.EditorStaticMeshLibrary.remove_collisions(asset)
            meshes.append(f"{a.asset_name} ({asset.get_num_triangles(0)} tris)")
    eal.save_directory(dest)
    print(f"[Wildreach] imported {name}: meshes {meshes}")
fix_materials("/Game/Props")
