"""Turns the Survivor Wraps' kilt the right way out (round 11 look pass, Raven: "fix the cloths and stuff on the player").

The kilt (the Legs pieces' "Linen" slot, on all six builds) was wound inside out: every triangle faced inward while its
normals pointed outward, so the camera saw the inside of the kilt's far side, shadowed by the body: a black skirt in
full sun. Each LOD of each SKM_Wraps_*_Legs: its kilt triangles reversed (flip_triangle_selection_normals reverses them
and their normals), then every vertex's normal put back as it was, written back with its bone weights. Run it once (a
second run would turn the kilt inside out again: it checks the winding first and skips a mesh already right).

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import unreal

L = unreal.GeometryScript_List
Q = unreal.GeometryScript_MeshQueries
log = []


def pick(r, kind):
    return next(x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, kind))


def kilt_inward(dm, kilt_id):
    """The share of the kilt's triangles wound inward (front = cross(c - a, b - a), measured against the hips' axis)."""
    vs = L.convert_vector_list_to_array(pick(Q.get_all_vertex_positions(dm, False), unreal.GeometryScriptVectorList))
    ts = L.convert_triangle_list_to_array(pick(Q.get_all_triangle_indices(dm, True), unreal.GeometryScriptTriangleList))
    ids = L.convert_index_list_to_array(pick(unreal.GeometryScript_Materials.get_all_triangle_material_i_ds(dm), unreal.GeometryScriptIndexList))
    inward = total = 0
    for t, mid in zip(ts, ids):
        if mid != kilt_id:
            continue
        a, b, c = vs[t.x], vs[t.y], vs[t.z]
        u = (c.x - a.x, c.y - a.y, c.z - a.z)
        w = (b.x - a.x, b.y - a.y, b.z - a.z)
        nx, ny = u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2]
        cx, cy = (a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0
        total += 1
        inward += 1 if nx * cx + ny * (cy - 2.0) < 0 else 0
    return inward / max(1, total)


sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
for build in ["M_Lean", "M_Athletic", "M_Strong", "F_Lean", "F_Athletic", "F_Strong"]:
    path = "/Game/Wildreach/Characters/Hero/Outfits/Wraps/SKM_Wraps_%s_Legs" % build
    sk = unreal.load_asset(path)
    names = [str(s.material_slot_name) for s in sk.materials]
    if "Linen" not in names:
        log.append("%s: no Linen slot" % build)
        continue
    kilt = names.index("Linen")
    lods = sub.get_lod_count(sk)
    for lod in range(lods):
        dm = unreal.DynamicMesh()
        read = unreal.GeometryScriptMeshReadLOD()
        read.set_editor_property("lod_index", lod)
        unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(sk, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), read)
        if not unreal.GeometryScript_BoneWeights.mesh_has_bone_weights(dm):
            log.append("%s LOD%d: no bone weights read, left alone" % (build, lod))
            continue
        share = kilt_inward(dm, kilt)
        if share < 0.5:
            log.append("%s LOD%d: kilt already outward (%.0f%% inward)" % (build, lod, share * 100))
            continue
        normals = pick(unreal.GeometryScript_Normals.get_mesh_per_vertex_normals(dm), unreal.GeometryScriptVectorList)
        sel = pick(unreal.GeometryScript_MeshSelection.select_mesh_elements_by_material_id(dm, kilt), unreal.GeometryScriptMeshSelection)
        unreal.GeometryScript_Normals.flip_triangle_selection_normals(dm, sel)
        unreal.GeometryScript_Normals.set_mesh_per_vertex_normals(dm, normals)
        after = kilt_inward(dm, kilt)
        opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        opts.set_editor_property("enable_recompute_normals", False)
        opts.set_editor_property("enable_recompute_tangents", True)
        opts.set_editor_property("replace_materials", False)
        opts.set_editor_property("use_original_vertex_order", True)
        opts.set_editor_property("remap_bone_indices_to_match_asset", True)
        write = unreal.GeometryScriptMeshWriteLOD()
        write.set_editor_property("lod_index", lod)
        _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(dm, sk, opts, write)
        log.append("%s LOD%d: kilt %.0f%% -> %.0f%% inward, %s" % (build, lod, share * 100, after * 100, outcome))
    unreal.EditorAssetLibrary.save_loaded_asset(sk, False)
unreal.log("[Wildreach] kilt: " + "; ".join(log))
with open(unreal.Paths.project_saved_dir() + "fix_wraps_kilt.txt", "w") as f:
    f.write("\n".join(log) + "\n")
