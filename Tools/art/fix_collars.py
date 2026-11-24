"""Reshapes two collars that came out of the hero lab as rings (round 13 of the look pass: a swim ring round the neck).
- Rimeguard's "huge white fur collar" (the chest's Rime_Fur ring, 70 cm across, 10 cm tall, level above the shoulders):
  now a fur mantle. Its outer edge drapes down over the shoulders and the upper back. It is deepest behind, opens and
  thins at the throat, and its surface breaks into tufts.
- Dunewrap's "wide hood down" (the head's Dune_Linen ring): now a lowered hood. It is bunched in soft folds behind
  the neck, gathers in round the sides and sags into a thin V at the front.
Only those pieces' vertices move (bone weights kept, normals recomputed over the piece and the rest left as they were),
on every LOD of every build. The deformation is a function of where a point sits round the ring, so each LOD gets the
same shape. Run it once on the lab's meshes (re-running deforms them again: `git checkout` the meshes first):
  UnrealEditor-Cmd <project> -run=pythonscript -script=<full path to this file>
Writes Saved/fix_collars.txt.
"""
import math
import unreal

L = unreal.GeometryScript_List
Q = unreal.GeometryScript_MeshQueries
N = unreal.GeometryScript_Normals
ROOT = "/Game/Wildreach/Characters/Hero/Outfits/"
BUILDS = ("M_Lean", "M_Athletic", "M_Strong", "F_Lean", "F_Athletic", "F_Strong")
JOBS = (("Rimeguard", "Chest", "Rime_Fur", "mantle"), ("Dunewrap", "Head", "Dune_Linen", "hood"))
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
log = []


def pick(r, kind):
    return next(x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, kind))


def read(sk, lod):
    dm = unreal.DynamicMesh()
    rl = unreal.GeometryScriptMeshReadLOD()
    rl.set_editor_property("lod_index", lod)
    unreal.GeometryScript_AssetUtils.copy_mesh_from_skeletal_mesh(sk, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), rl)
    return dm


def pieces(ts, ids, nv):
    parent = list(range(nv))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    for t in ts:
        ra = root(t.x)
        parent[root(t.y)] = ra
        parent[root(t.z)] = ra
    out = {}
    for ti, (t, mid) in enumerate(zip(ts, ids)):
        p = out.setdefault(root(t.x), {"mid": mid, "tris": [], "v": set()})
        p["tris"].append(ti)
        p["v"].update((t.x, t.y, t.z))
    return list(out.values())


def ring_of(vs, ps, slot_id, geo=None):
    """The ring: the biggest piece in the slot that is flat (under 16 cm tall) and wide (over 25 cm out from its
    middle). On a reduced LOD (geo: LOD0's ring) the piece of the slot lying most inside LOD0's ring; where the
    reduction joined the ring to another piece of the slot (Rimeguard's cloak hangs from its back), the slot's
    vertices inside the ring and the triangles between them."""
    if geo is not None:
        def within(i):
            return abs(math.hypot(vs[i].x - geo["cx"], vs[i].y - geo["cy"]) - geo["R0"]) <= geo["a"] + 2.5 and abs(vs[i].z - geo["z0"]) <= geo["b"] + 2.5
        best, most = None, 0
        for p in ps:
            if p["mid"] != slot_id:
                continue
            inside = sum(1 for i in p["v"] if within(i))
            if inside >= 0.7 * len(p["v"]) and inside > most:
                best, most = p, inside
        if best:
            return best, geo
        vin = set(i for p in ps if p["mid"] == slot_id for i in p["v"] if within(i))
        tris = [ti for p in ps if p["mid"] == slot_id for ti in p["tris"] if all(i in vin for i in TRI[ti])]
        return ({"mid": slot_id, "tris": tris, "v": vin}, geo) if vin else None
    best = None
    for p in ps:
        if p["mid"] != slot_id or len(p["tris"]) < 60:
            continue
        pts = [vs[i] for i in p["v"]]
        zs = [q.z for q in pts]
        cx = (min(q.x for q in pts) + max(q.x for q in pts)) / 2
        cy = (min(q.y for q in pts) + max(q.y for q in pts)) / 2
        rs = [math.hypot(q.x - cx, q.y - cy) for q in pts]
        if max(zs) - min(zs) > 16.0 or max(rs) < 25.0:
            continue
        if best is None or len(p["tris"]) > len(best[0]["tris"]):
            best = (p, {"cx": cx, "cy": cy, "z0": (min(zs) + max(zs)) / 2, "b": max(0.5, (max(zs) - min(zs)) / 2),
                        "R0": (min(rs) + max(rs)) / 2, "a": max(0.5, (max(rs) - min(rs)) / 2)})
    return best


def smooth(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def mantle(p, g):
    """Rimeguard: a fur mantle over the shoulders."""
    dx, dy = p.x - g["cx"], p.y - g["cy"]
    th = math.atan2(dy, dx)
    r = math.hypot(dx, dy)
    front, back, side = max(0.0, math.sin(th)), max(0.0, -math.sin(th)), abs(math.cos(th))
    u = (r - g["R0"]) / g["a"]          # -1 inner .. 1 outer
    v = (p.z - g["z0"]) / g["b"]        # -1 under .. 1 over
    out = (u + 1) / 2
    # thicker behind and over the shoulders, thin at the throat
    # (its front stood 18 cm out from the chest, a bar from shoulder to shoulder: pulled in onto the chest, a trim)
    k = 1.0 + 0.55 * back + 0.35 * side - 0.65 * front
    a, b = g["a"] * (1.0 - 0.6 * front), g["b"] * k
    # tufts: the tube's surface pushed out and in, in locks round the ring
    phi = math.atan2(v, u)
    tuft = 1.3 * math.sin(th * 29.0 + 2.3 * math.sin(th * 7.0)) * math.sin(phi * 5.0 + th * 11.0) + 0.5 * math.sin(th * 61.0 + phi * 3.0)
    rr = g["R0"] * (1.0 - 0.33 * front ** 1.5) + u * a + (2.5 + 3.0 * side + 2.5 * back) * out * (1.0 - front) + tuft * math.cos(phi) * 0.8
    zz = g["z0"] + v * b + tuft * math.sin(phi) * 0.8
    # the outer edge drapes down over the shoulders and the back; the front lies lower, open at the throat
    zz -= (out ** 1.5) * (7.0 * side + 11.0 * back + 5.0 * front) + 6.0 * front
    return unreal.Vector(g["cx"] + math.cos(th) * rr, g["cy"] + math.sin(th) * rr, zz)


def hood(p, g):
    """Dunewrap: a hood lowered onto the shoulders."""
    dx, dy = p.x - g["cx"], p.y - g["cy"]
    th = math.atan2(dy, dx)
    r = math.hypot(dx, dy)
    front, back, side = max(0.0, math.sin(th)), max(0.0, -math.sin(th)), abs(math.cos(th))
    u = (r - g["R0"]) / g["a"]
    v = (p.z - g["z0"]) / g["b"]
    out = (u + 1) / 2
    # gathered in toward the neck (it stood out past the shoulders), bunched tall behind in folds, thin in front
    fold = 1.0 + 0.22 * math.sin(th * 7.0 + 0.6) * (side + back)
    a = g["a"] * 0.72 * (1.0 - 0.55 * front) * fold
    b = g["b"] * (1.0 + 0.7 * back + 0.3 * side - 0.5 * front) * fold
    rr = g["R0"] * 0.9 + u * a + 3.0 * back
    zz = g["z0"] + v * b - (out ** 1.3) * (9.0 * back + 3.0 * side) - 9.0 * front ** 2 * (0.6 + 0.4 * out)
    return unreal.Vector(g["cx"] + math.cos(th) * rr, g["cy"] + math.sin(th) * rr, zz)


SHAPES = {"mantle": mantle, "hood": hood}
TRI = []
for setname, slot, slot_name, shape in JOBS:
    for build in BUILDS:
        path = f"{ROOT}{setname}/SKM_{setname}_{build}_{slot}"
        sk = unreal.load_asset(path)
        if not sk:
            log.append(f"{path}: missing")
            continue
        names = [str(s.material_slot_name) for s in sk.materials]
        if slot_name not in names:
            log.append(f"{path}: no slot {slot_name}")
            continue
        sid = names.index(slot_name)
        geo = None
        for lod in range(sub.get_lod_count(sk)):
            dm = read(sk, lod)
            vs = L.convert_vector_list_to_array(pick(Q.get_all_vertex_positions(dm, False), unreal.GeometryScriptVectorList))
            ts = L.convert_triangle_list_to_array(pick(Q.get_all_triangle_indices(dm, True), unreal.GeometryScriptTriangleList))
            ids = L.convert_index_list_to_array(pick(unreal.GeometryScript_Materials.get_all_triangle_material_i_ds(dm), unreal.GeometryScriptIndexList))
            TRI.clear()
            TRI.extend((t.x, t.y, t.z) for t in ts)
            found = ring_of(vs, pieces(ts, ids, len(vs)), sid, geo)
            if not found:
                log.append(f"{path} LOD{lod}: no ring")
                continue
            piece, g = found
            if geo is None:
                geo = g          # LOD0's ring: every LOD is reshaped about it
            old_n = L.convert_vector_list_to_array(pick(N.get_mesh_per_vertex_normals(dm), unreal.GeometryScriptVectorList))
            moved = {}
            for i in piece["v"]:
                moved[i] = SHAPES[shape](vs[i], geo)
                unreal.GeometryScript_MeshEdits.set_vertex_position(dm, i, moved[i], True)
            # the piece's normals from its new faces (each kept on the side its old one faced)
            acc = {i: [0.0, 0.0, 0.0] for i in piece["v"]}
            for ti in piece["tris"]:
                t = ts[ti]
                a, b, c = moved[t.x], moved[t.y], moved[t.z]
                e1 = (b.x - a.x, b.y - a.y, b.z - a.z)
                e2 = (c.x - a.x, c.y - a.y, c.z - a.z)
                f = (e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0])
                for i in (t.x, t.y, t.z):
                    acc[i][0] += f[0]
                    acc[i][1] += f[1]
                    acc[i][2] += f[2]
            new_n = list(old_n)
            for i, s in acc.items():
                ln = math.sqrt(s[0] ** 2 + s[1] ** 2 + s[2] ** 2) or 1.0
                n = unreal.Vector(s[0] / ln, s[1] / ln, s[2] / ln)
                o = old_n[i]
                if n.x * o.x + n.y * o.y + n.z * o.z < 0:
                    n = unreal.Vector(-n.x, -n.y, -n.z)
                new_n[i] = n
            N.set_mesh_per_vertex_normals(dm, L.convert_array_to_vector_list(new_n))
            opts = unreal.GeometryScriptCopyMeshToAssetOptions()
            opts.set_editor_property("enable_recompute_normals", False)
            opts.set_editor_property("enable_recompute_tangents", True)
            opts.set_editor_property("replace_materials", False)
            opts.set_editor_property("use_original_vertex_order", True)
            opts.set_editor_property("remap_bone_indices_to_match_asset", True)
            wl = unreal.GeometryScriptMeshWriteLOD()
            wl.set_editor_property("lod_index", lod)
            _, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_to_skeletal_mesh(dm, sk, opts, wl)
            log.append(f"{path} LOD{lod}: {len(piece['v'])} vertices of the {slot_name} ring reshaped ({shape}), {outcome}")
        unreal.EditorAssetLibrary.save_loaded_asset(sk, False)
with open(unreal.Paths.project_saved_dir() + "fix_collars.txt", "w") as f:
    f.write("\n".join(log) + "\n")
