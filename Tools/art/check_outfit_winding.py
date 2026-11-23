"""Finds outfit parts built inside out: for every hero outfit skeletal mesh (Content/.../Hero/Outfits/*) and each material
slot and each piece of it (triangles joined by shared vertices), how many triangles are wound against their own vertex normals (a triangle's front is cross(c - a, b - a) in a
DynamicMesh; its three normals should lean the same way). A piece whose triangles mostly disagree is inside out: the
camera sees through its near side to the shadowed inside of its far side (round 11: the Survivor Wraps' kilt, black in
full sun). WR_FIX=1 also reverses such pieces (normals and bone weights kept) on every LOD, as fix_wraps_kilt.py did.

Writes Saved/check_outfit_winding.txt. Run headless:
  UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
"""
import os
import unreal

L = unreal.GeometryScript_List
Q = unreal.GeometryScript_MeshQueries
FIX = os.environ.get("WR_FIX") == "1"
ROOT = "/Game/Wildreach/Characters/Hero/Outfits"
log = []
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)


def pick(r, kind):
    return next(x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, kind))


def read(sk, lod):
    dm = unreal.DynamicMesh()
    rl = unreal.GeometryScriptMeshReadLOD()
    rl.set_editor_property("lod_index", lod)
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(sk, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), rl)
    return dm


def against(dm):
    """Per (material id, piece): [triangles, triangles wound against their normals, their triangle ids]; a piece is a
    set of triangles joined by shared vertices (a coat's tails are pieces of its own, one may be inside out alone)."""
    vs = L.convert_vector_list_to_array(pick(Q.get_all_vertex_positions(dm, False), unreal.GeometryScriptVectorList))
    ns = L.convert_vector_list_to_array(pick(unreal.GeometryScript_Normals.get_mesh_per_vertex_normals(dm), unreal.GeometryScriptVectorList))
    ts = L.convert_triangle_list_to_array(pick(Q.get_all_triangle_indices(dm, True), unreal.GeometryScriptTriangleList))
    ids = L.convert_index_list_to_array(pick(unreal.GeometryScript_Materials.get_all_triangle_material_i_ds(dm), unreal.GeometryScriptIndexList))
    parent = list(range(len(vs)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for t in ts:
        ra, rb, rc = root(t.x), root(t.y), root(t.z)
        parent[rb] = ra
        parent[root(rc)] = ra
    out = {}
    for ti, (t, mid) in enumerate(zip(ts, ids)):
        a, b, c = vs[t.x], vs[t.y], vs[t.z]
        u = (c.x - a.x, c.y - a.y, c.z - a.z)
        w = (b.x - a.x, b.y - a.y, b.z - a.z)
        f = (u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0])
        n = (ns[t.x].x + ns[t.y].x + ns[t.z].x, ns[t.x].y + ns[t.y].y + ns[t.z].y, ns[t.x].z + ns[t.y].z + ns[t.z].z)
        s = out.setdefault((mid, root(t.x)), [0, 0, []])
        s[0] += 1
        s[1] += 1 if f[0] * n[0] + f[1] * n[1] + f[2] * n[2] < 0 else 0
        s[2].append(ti)
    return out


ar = unreal.AssetRegistryHelpers.get_asset_registry()
assets = [a for a in ar.get_assets_by_path(ROOT, recursive=True) if str(a.asset_class_path.asset_name) == "SkeletalMesh"]
bad = 0
for a in sorted(assets, key=lambda x: str(x.package_name)):
    sk = a.get_asset()
    names = [str(s.material_slot_name) for s in sk.materials]
    dm = read(sk, 0)
    if not unreal.GeometryScript_BoneWeights.mesh_has_bone_weights(dm):
        continue
    res = against(dm)
    # per slot: the pieces inside out (most of a piece's triangles against its normals; at least 12 triangles)
    slots = {}
    for (mid, _), (n, wrong, tris) in res.items():
        s = slots.setdefault(mid, [0, 0, 0, 0])
        s[0] += n
        if n >= 12 and wrong / n > 0.6:
            s[1] += n
            s[2] += 1
        s[3] += 1
    flagged = False
    for mid, (n, inside, pieces_in, pieces) in sorted(slots.items()):
        if inside:
            flagged = True
            log.append("%s %s: %d of %d pieces inside out (%d of %d triangles)" % (str(a.package_name).split("/")[-1], names[mid] if mid < len(names) else mid, pieces_in, pieces, inside, n))
    if flagged:
        bad += 1
    if FIX and flagged:
        for lod in range(sub.get_lod_count(sk)):
            dl = read(sk, lod)
            todo = [tri for (mid, _), (n, wrong, tris) in against(dl).items() if n >= 12 and wrong / n > 0.6 for tri in tris]
            if not todo:
                log.append("   LOD%d already right" % lod)
                continue
            normals = pick(unreal.GeometryScript_Normals.get_mesh_per_vertex_normals(dl), unreal.GeometryScriptVectorList)
            sel = pick(unreal.GeometryScript_MeshSelection.convert_index_array_to_mesh_selection(dl, todo, unreal.GeometryScriptMeshSelectionType.TRIANGLES), unreal.GeometryScriptMeshSelection)
            unreal.GeometryScript_Normals.flip_triangle_selection_normals(dl, sel)
            unreal.GeometryScript_Normals.set_mesh_per_vertex_normals(dl, normals)
            opts = unreal.GeometryScriptCopyMeshToAssetOptions()
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            opts.set_editor_property("replace_materials", False)
            opts.set_editor_property("use_original_vertex_order", True)
            opts.set_editor_property("remap_bone_indices_to_match_asset", True)
            wl = unreal.GeometryScriptMeshWriteLOD()
            wl.set_editor_property("lod_index", lod)
            _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(dl, sk, opts, wl)
            log.append("   fixed LOD%d: %d triangles turned, %s" % (lod, len(todo), outcome))
        unreal.EditorAssetLibrary.save_loaded_asset(sk, False)
log.append("%d meshes checked, %d with a part inside out" % (len(assets), bad))
with open(unreal.Paths.project_saved_dir() + "check_outfit_winding.txt", "w") as f:
    f.write("\n".join(log) + "\n")
