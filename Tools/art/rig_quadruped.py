# Blender (headless): rig an image-to-3D quadruped and bake its basic animations, then export FBX for Unreal.
#   blender -b -P rig_quadruped.py -- <model.glb> <out.fbx> [--head +x|-x|+y|-y] [--gait trot|hop] [--legs 2|4|6] [--add-mid-legs] [--stretch 1.3]
#       [--drop-leg x,y] [--mirror-leg x,y] [--straighten-front] [--legs-longer 1.2] [--legs-thicker 1.15]
#       [--head-forward 0.08] [--widen 1.25] [--front-wrist 0.11 | --front-elbow [0.4]] [--front-hip-x 0.2] [--hip-z 0.5] [--belly-z 0.3]
#       [--preview out.png]
# Legs are found by clustering the lowest part of the mesh; spine/neck/head/tail come from the leg positions.
# Actions: Idle, Walk, Run, Attack, Hit, Death (30 fps). Unreal gets one skeletal mesh + six animations.
import bpy, sys, math
import numpy as np
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
head_dir = args[args.index("--head") + 1] if "--head" in args else "+x"
preview = args[args.index("--preview") + 1] if "--preview" in args else None
gait_kind = args[args.index("--gait") + 1] if "--gait" in args else "trot"  # trot | hop (front pair, then rear pair)
stretch = float(args[args.index("--stretch") + 1]) if "--stretch" in args else 1.0  # torso length multiplier
legs_n = int(args[args.index("--legs") + 1]) if "--legs" in args else 4  # 4, or 6 (front / middle / rear pairs)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
body = bpy.context.view_layer.objects.active
for o in [o for o in bpy.context.scene.objects if o.type != "MESH"]:
    bpy.data.objects.remove(o)
bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
# Face +X, stand on z=0, 1 unit tall (Unreal fits the scale to the creature's capsule).
yaw = {"+x": 0, "-x": math.pi, "+y": -math.pi / 2, "-y": math.pi / 2}[head_dir]
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
body.data.transform(Matrix.Rotation(yaw, 4, "Z"))  # the glTF importer leaves objects in quaternion mode, so rotation_euler is ignored
v = np.array([body.matrix_world @ p.co for p in body.data.vertices])
lo, hi = v.min(0), v.max(0)
s = 1.0 / (hi[2] - lo[2])
body.data.transform(Matrix.Translation(Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))))
body.data.transform(Matrix.Scale(s, 4))
body.location = (0, 0, 0)
bpy.ops.object.shade_smooth()
# --widen 1.25: a body too narrow side to side (image models of a creature seen side-on guess its width)
if "--widen" in args:
    body.data.transform(Matrix.Scale(float(args[args.index("--widen") + 1]), 4, Vector((0, 1, 0))))
v = np.array([p.co[:] for p in body.data.vertices])
lo, hi = v.min(0), v.max(0)
L = hi[0] - lo[0]

# ---- legs: k-means on the low vertices, seeded by the four quadrants
def find_feet(cut, count=None):
    """Rasterize the vertices below `cut` onto a 1.5 cm grid and return the centres of the 4 biggest islands."""
    count = count or legs_n
    low = v[v[:, 2] < cut][:, :2]
    cell = 0.015
    ij = np.floor((low - low.min(0)) / cell).astype(int)
    occ = {}
    for n, key in enumerate(map(tuple, ij)):
        occ.setdefault(key, []).append(n)
    seen, islands = set(), []
    for start in occ:
        if start in seen:
            continue
        stack, members = [start], []
        seen.add(start)
        while stack:
            c = stack.pop()
            members += occ[c]
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    nb = (c[0] + dx, c[1] + dy)
                    if nb in occ and nb not in seen:
                        seen.add(nb)
                        stack.append(nb)
        islands.append(members)
    islands.sort(key=len, reverse=True)
    return [low[m].mean(0) for m in islands[:count]], [len(m) for m in islands[:count + 2]]

def feet_now():
    for cut in (0.06, 0.1, 0.15, 0.2):  # lowest slice that shows every foot separately
        feet, sizes = find_feet(cut)
        # ...and only the feet: split toes (a rhino's three) show as more islands as big as the feet: go higher
        clear = len(sizes) <= legs_n or sizes[legs_n] < 0.4 * sizes[legs_n - 1]
        if len(feet) == legs_n and sizes[legs_n - 1] > 20 and clear:
            return np.array(feet), cut, sizes
    if legs_n == 2:
        # A bird mid-stride has one foot in front of the other and the toes overlap: split the lowest slice along X.
        low = v[v[:, 2] < 0.06][:, :2]
        a, b = low[:, 0].min(), low[:, 0].max()
        for _ in range(12):
            near_a = np.abs(low[:, 0] - a) < np.abs(low[:, 0] - b)
            a, b = low[near_a, 0].mean(), low[~near_a, 0].mean()
        return np.array([low[near_a].mean(0), low[~near_a].mean(0)]), 0.06, [int(near_a.sum()), int((~near_a).sum())]
    return np.array(feet), cut, sizes

# Image-to-3D creatures often stand at an angle (three-quarter concept art): turn the body so the line from the
# rear feet to the front feet runs along +X, then find the feet again.
def align():
    global v, lo, hi, L, centers, cut, sizes
    centers, cut, sizes = feet_now()
    by_x = centers[np.argsort(-centers[:, 0])]
    fwd = by_x[:2].mean(0) - by_x[-2:].mean(0)
    turn = -math.atan2(fwd[1], fwd[0])
    if abs(turn) > math.radians(3):
        body.data.transform(Matrix.Rotation(turn, 4, "Z"))
        v = np.array([p.co[:] for p in body.data.vertices])
        mid = (v.min(0) + v.max(0)) / 2
        body.data.transform(Matrix.Translation(Vector((-mid[0], -mid[1], 0))))
        v = np.array([p.co[:] for p in body.data.vertices])
        lo, hi = v.min(0), v.max(0)
        L = hi[0] - lo[0]
        centers, cut, sizes = feet_now()
    print("aligned by", round(math.degrees(turn), 1), "deg")


align()
# Image models sometimes draw a leg too many (Cinderfang's concept asked for six legs and got five): --drop-leg x,y
# (where that foot stands once aligned, as the "legs" line prints) cuts off the leg standing there below the belly,
# caps the hole and aligns the body again on the legs that are left.
if "--drop-leg" in args:
    import bmesh
    drop = np.array([float(c) for c in args[args.index("--drop-leg") + 1].split(",")])
    others = [c for c in find_feet(0.06, legs_n + 2)[0] if np.hypot(*(c - drop)) > 0.06]
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)  # textured meshes are split at UV seams: weld so a leg is one piece
    faces = list(bm.faces)
    fc = np.array([f.calc_center_median()[:] for f in faces])
    ftop = np.array([max(p.co.z for p in f.verts) for f in faces])
    index = {f: i for i, f in enumerate(faces)}

    def piece_below(cut_z):
        below = ftop < cut_z
        d = np.where(below, (fc[:, 0] - drop[0]) ** 2 + (fc[:, 1] - drop[1]) ** 2 + fc[:, 2] ** 2, np.inf)
        start = int(np.argmin(d))
        seen, stack = {start}, [start]
        while stack:
            for e in faces[stack.pop()].edges:
                for h in e.link_faces:
                    j = index[h]
                    if below[j] and j not in seen:
                        seen.add(j)
                        stack.append(j)
        ids = np.fromiter(seen, int)
        joined = any(np.any((np.hypot(fc[ids, 0] - o[0], fc[ids, 1] - o[1]) < 0.06) & (fc[ids, 2] < 0.08)) for o in others)
        return ids, joined

    def spread(ids):
        return float(max(np.ptp(fc[ids, 0]), np.ptp(fc[ids, 1])))

    cut_z = 0.35
    ids, joined = piece_below(cut_z)
    while joined and cut_z > 0.1:  # reaching another foot means the slice took in belly: cut lower
        cut_z -= 0.02
        ids, joined = piece_below(cut_z)
    while cut_z < 0.8:  # then all the way up to where it meets the body, so no stump is left hanging
        up, up_joined = piece_below(cut_z + 0.01)
        if up_joined or spread(up) > spread(ids) * 1.25 + 0.02:
            break
        ids, cut_z = up, cut_z + 0.01
    gone = [faces[j] for j in ids]
    rim = {e for f in gone for e in f.edges}
    bmesh.ops.delete(bm, geom=gone, context="FACES_ONLY")
    rim = [e for e in rim if e.is_valid and len(e.link_faces) == 1]
    capped = bmesh.ops.holes_fill(bm, edges=rim, sides=0)["faces"]
    bmesh.ops.triangulate(bm, faces=capped)
    bmesh.ops.delete(bm, geom=[p for p in bm.verts if not p.link_faces], context="VERTS")
    bm.to_mesh(body.data)
    bm.free()
    body.data.update()
    v = np.array([p.co[:] for p in body.data.vertices])
    print("dropped the leg at", np.round(drop, 3), "cut", round(cut_z, 2), "faces", len(gone), "hole capped with", len(capped))
    align()
# Three-quarter concepts come out foreshortened (short bodies): --stretch lengthens only the torso between the inner
# edges of the rear and front feet, so legs, head and tail keep their shape.
if stretch != 1.0:
    xs = np.sort(centers[:, 0])
    rx0, fx0 = xs[1] + 0.05, xs[-2] - 0.05
    if fx0 > rx0:
        x = v[:, 0]
        nx = np.where(x < rx0, x, np.where(x > fx0, x + (fx0 - rx0) * (stretch - 1), rx0 + (x - rx0) * stretch))
        v = v.copy()
        v[:, 0] = nx - (nx.min() + nx.max()) / 2
        body.data.vertices.foreach_set("co", v.ravel())
        body.data.update()
        lo, hi = v.min(0), v.max(0)
        L = hi[0] - lo[0]
        centers, cut, sizes = feet_now()
    print("torso stretched", stretch, "between x", round(float(rx0), 2), round(float(fx0), 2))
# Six-legged mammals: image models only draw four legs, so --add-mid-legs copies the front pair (with its UVs) into a
# middle pair halfway back, lengthened just enough for the tops to tuck into the belly.
if "--add-mid-legs" in args and legs_n == 4:
    import bmesh
    by = centers[np.argsort(-centers[:, 0])]
    fx_, rx_ = by[:2, 0].mean(), by[2:, 0].mean()
    mid_x = (fx_ + rx_) / 2
    band = v[(np.abs(v[:, 0] - mid_x) < 0.05) & (np.abs(v[:, 1]) < 0.1)]
    belly_mid = float(np.percentile(band[:, 2], 3)) if len(band) else 0.3  # underside halfway between the legs
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)  # textured meshes are split at UV seams: weld so a leg is one piece
    faces = list(bm.faces)
    fc = np.array([f.calc_center_median()[:] for f in faces])
    ftop = np.array([max(p.co.z for p in f.verts) for f in faces])
    index = {f: i for i, f in enumerate(faces)}

    def leg_piece(foot, other, cut):
        """Faces below `cut` connected to this foot; True if that piece also reaches the other front foot."""
        below = ftop < cut
        d = np.where(below, (fc[:, 0] - foot[0]) ** 2 + (fc[:, 1] - foot[1]) ** 2 + fc[:, 2] ** 2, np.inf)
        start = int(np.argmin(d))
        seen, stack = {start}, [start]
        while stack:
            for e in faces[stack.pop()].edges:
                for h in e.link_faces:
                    j = index[h]
                    if below[j] and j not in seen:
                        seen.add(j)
                        stack.append(j)
        ids = np.fromiter(seen, int)
        reach = np.any((np.hypot(fc[ids, 0] - other[0], fc[ids, 1] - other[1]) < 0.06) & (fc[ids, 2] < 0.08))
        return [faces[j] for j in ids], bool(reach)

    for target, opposite in ((by[0], by[1]), (by[1], by[0])):
        # Three-quarter concepts splay the far front leg out to the side; a copy of it would stand outside the body.
        # Copy the front leg that stands under the body instead, mirrored onto this side.
        foot, other, mirror = target, opposite, False
        if abs(target[1]) > 1.6 * abs(opposite[1]) + 0.05 and target[1] * opposite[1] < 0:
            foot, other, mirror = opposite, target, True
        side_y = -foot[1] if mirror else foot[1]
        cut = belly_mid
        piece, joined = leg_piece(foot, other, cut)
        while joined and cut > 0.12:  # a deep chest joins the front legs: cut lower until this one stands alone
            cut -= 0.02
            piece, joined = leg_piece(foot, other, cut)
        near = v[(np.abs(v[:, 0] - mid_x) < 0.04) & (np.abs(v[:, 1] - side_y) < 0.08)]
        under = float(np.percentile(near[:, 2], 3)) if len(near) else belly_mid  # the belly right above the new leg
        lift = min(1.5, max(1.0, (under + 0.04) / cut))
        dup = bmesh.ops.duplicate(bm, geom=piece)
        for p in (g for g in dup["geom"] if isinstance(g, bmesh.types.BMVert)):
            p.co.x += mid_x - foot[0]
            p.co.z *= lift
            if mirror:
                p.co.y = -p.co.y
        if mirror:
            bmesh.ops.reverse_faces(bm, faces=[g for g in dup["geom"] if isinstance(g, bmesh.types.BMFace)])
        print("middle leg from", [round(float(c), 2) for c in foot[:2]], "mirrored" if mirror else "", "cut", round(cut, 2),
              "faces", len(piece), "lift", round(lift, 2))
    bm.to_mesh(body.data)
    bm.free()
    body.data.update()
    legs_n = 6
    v = np.array([p.co[:] for p in body.data.vertices])
    lo, hi = v.min(0), v.max(0)
    L = hi[0] - lo[0]
    centers, cut, sizes = feet_now()
# Leg shaping, on the mesh itself: image models give crooked legs (a front leg kinked like an S reads as an extra joint
# however the bones move it). --straighten-front stands each front leg as a column under where it meets the body;
# --legs-longer 1.2 lengthens every leg (the body rides up on them, so a deep chest stops reaching down between the
# front legs); --legs-thicker 1.15 fattens them about their centre lines. The model is scaled back to 1 unit tall.
legs_longer = float(args[args.index("--legs-longer") + 1]) if "--legs-longer" in args else 1.0
legs_thicker = float(args[args.index("--legs-thicker") + 1]) if "--legs-thicker" in args else 1.0
straighten = "--straighten-front" in args
# --head-forward 0.08: a head jammed against the body (Basaltback's ears and tusks clip its shell) comes forward on a
# longer neck, the legs staying where they are.
head_fwd = float(args[args.index("--head-forward") + 1]) if "--head-forward" in args else 0.0
front_wrist = float(args[args.index("--front-wrist") + 1]) if "--front-wrist" in args else 0.0
front_hip_x = float(args[args.index("--front-hip-x") + 1]) if "--front-hip-x" in args else None
front_elbow = "--front-elbow" in args
_i = args.index("--front-elbow") + 1 if front_elbow else 0
front_elbow_z = float(args[_i]) if front_elbow and _i < len(args) and not args[_i].startswith("--") else 0.0
if straighten or legs_longer != 1.0 or legs_thicker != 1.0 or head_fwd:
    import bmesh
    from mathutils import kdtree
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    bm.verts.ensure_lookup_table()
    faces = list(bm.faces)
    fc = np.array([f.calc_center_median()[:] for f in faces])
    ftop = np.array([max(p.co.z for p in f.verts) for f in faces])
    index = {f: i for i, f in enumerate(faces)}
    wv = np.array([p.co[:] for p in bm.verts])
    feet = list(centers)

    def piece_below(foot, cut_z):
        below = ftop < cut_z
        d = np.where(below, (fc[:, 0] - foot[0]) ** 2 + (fc[:, 1] - foot[1]) ** 2 + fc[:, 2] ** 2, np.inf)
        start = int(np.argmin(d))
        seen, stack = {start}, [start]
        while stack:
            for e in faces[stack.pop()].edges:
                for h in e.link_faces:
                    j = index[h]
                    if below[j] and j not in seen:
                        seen.add(j)
                        stack.append(j)
        ids = np.fromiter(seen, int)
        joined = any(np.any((np.hypot(fc[ids, 0] - o[0], fc[ids, 1] - o[1]) < 0.06) & (fc[ids, 2] < 0.08))
                     for o in feet if np.hypot(o[0] - foot[0], o[1] - foot[1]) > 0.06)
        return ids, joined

    def spread(ids):
        return float(max(np.ptp(fc[ids, 0]), np.ptp(fc[ids, 1])))

    front = set(int(k) for k in np.argsort(-np.array([f[0] for f in feet]))[:2]) if len(feet) >= 4 else set()
    leg_of = np.full(len(wv), -1)
    tops, lines = [], []
    for k, foot in enumerate(feet):
        cut_z = 0.35
        ids, joined = piece_below(foot, cut_z)
        while joined and cut_z > 0.1:
            cut_z -= 0.02
            ids, joined = piece_below(foot, cut_z)
        while cut_z < 0.8:  # up to where it meets the body
            up, up_joined = piece_below(foot, cut_z + 0.01)
            if up_joined or spread(up) > spread(ids) * 1.25 + 0.02:
                break
            ids, cut_z = up, cut_z + 0.01
        vids = np.array(sorted({p.index for j in ids for p in faces[j].verts}))
        pts = wv[vids]
        zs = np.arange(0.01, cut_z, 0.02)
        line = np.array([np.median(pts[np.abs(pts[:, 2] - z) < 0.015][:, :2], 0) if np.any(np.abs(pts[:, 2] - z) < 0.015)
                         else foot[:2] for z in zs])
        # Only what's round the leg's centre line is leg: a tusk or a tail tip resting against it is welded on too.
        cx = np.interp(pts[:, 2], zs, line[:, 0])
        cy = np.interp(pts[:, 2], zs, line[:, 1])
        r = np.hypot(pts[:, 0] - cx, pts[:, 1] - cy)
        keep = r < 2.0 * float(np.median(r)) + 0.01
        leg_of[vids[keep]] = k
        tops.append(cut_z)
        lines.append((zs, line))
        print(f"leg shape {k}: {'front' if k in front else 'rear'} foot {np.round(foot[:2], 3)} meets the body at z {cut_z:.2f}")
    rise = (legs_longer - 1.0) * float(np.mean(tops))  # how far the body rides up
    tree = kdtree.KDTree(len(wv))
    for i, co in enumerate(wv):
        tree.insert(co, i)
    tree.balance()
    co = np.array([p.co[:] for p in body.data.vertices])
    new = co.copy()
    # The neck starts at the front feet; everything past it (not a leg) slides forward, easing in over a tenth of a unit.
    neck_x = float(np.mean([feet[k][0] for k in front])) - 0.05 if front else float(co[:, 0].max())
    for i, c in enumerate(co):
        k = leg_of[tree.find(c)[1]]
        if k < 0:
            new[i, 2] = c[2] + rise
            if head_fwd:
                t = min(1.0, max(0.0, (c[0] - neck_x) / 0.1))
                new[i, 0] += head_fwd * t * t * (3 - 2 * t)
            continue
        top = tops[k]
        zs, line = lines[k]
        z = min(max(c[2], zs[0]), zs[-1])
        cx = np.interp(z, zs, line[:, 0])
        cy = np.interp(z, zs, line[:, 1])
        t = min(1.0, max(0.0, (c[2] - 0.7 * top) / (0.3 * top)))
        w = 1.0 - t * t * (3 - 2 * t)  # full below 70% of the leg, easing out to nothing where it meets the body
        if straighten and k in front:
            new[i, 0] += (np.interp(zs[-1], zs, line[:, 0]) - cx) * w
        new[i, 0] += (c[0] - cx) * (legs_thicker - 1.0) * w
        new[i, 1] += (c[1] - cy) * (legs_thicker - 1.0) * w
        new[i, 2] = c[2] * (1.0 + rise / top)
    bm.free()
    new[:, 2] -= new[:, 2].min()
    new /= new[:, 2].max()
    new[:, 0] -= (new[:, 0].min() + new[:, 0].max()) / 2
    new[:, 1] -= (new[:, 1].min() + new[:, 1].max()) / 2
    body.data.vertices.foreach_set("co", new.ravel())
    body.data.update()
    v = new
    lo, hi = v.min(0), v.max(0)
    L = hi[0] - lo[0]
    centers, cut, sizes = feet_now()
    print(f"legs shaped: straightened front {straighten}, longer x{legs_longer} (body up {rise:.3f}), thicker x{legs_thicker}, "
          f"head forward {head_fwd} from x {neck_x:.2f}")
# A leg that grew from the wrong place (Cinderfang's front right is what's left of a sixth leg, rooted behind the
# shoulder): --mirror-leg x,y cuts off the leg standing there and puts a mirror copy of the leg opposite in its place
# (after any leg shaping, so the copy is the shaped leg).
if "--mirror-leg" in args:
    import bmesh
    spot = np.array([float(c) for c in args[args.index("--mirror-leg") + 1].split(",")])
    all_feet = list(centers)
    spot = min(all_feet, key=lambda c: np.hypot(c[0] - spot[0], c[1] - spot[1]))  # where that foot stands now (shaping moves feet)
    src_foot = min(all_feet, key=lambda c: np.hypot(c[0] - spot[0], c[1] + spot[1]))
    bm = bmesh.new()
    bm.from_mesh(body.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    faces = list(bm.faces)
    fc = np.array([f.calc_center_median()[:] for f in faces])
    ftop = np.array([max(p.co.z for p in f.verts) for f in faces])
    index = {f: i for i, f in enumerate(faces)}

    def piece_below(foot, cut_z):
        below = ftop < cut_z
        d = np.where(below, (fc[:, 0] - foot[0]) ** 2 + (fc[:, 1] - foot[1]) ** 2 + fc[:, 2] ** 2, np.inf)
        start = int(np.argmin(d))
        seen, stack = {start}, [start]
        while stack:
            for e in faces[stack.pop()].edges:
                for h in e.link_faces:
                    j = index[h]
                    if below[j] and j not in seen:
                        seen.add(j)
                        stack.append(j)
        ids = np.fromiter(seen, int)
        joined = any(np.any((np.hypot(fc[ids, 0] - o[0], fc[ids, 1] - o[1]) < 0.06) & (fc[ids, 2] < 0.08))
                     for o in all_feet if np.hypot(o[0] - foot[0], o[1] - foot[1]) > 0.06)
        return ids, joined

    def whole_leg(foot):
        """The leg standing on `foot`, from the ground up to where it meets the body (as --drop-leg finds it)."""
        cut_z = 0.35
        ids, joined = piece_below(foot, cut_z)
        while joined and cut_z > 0.1:
            cut_z -= 0.02
            ids, joined = piece_below(foot, cut_z)
        while cut_z < 0.8:
            up, up_joined = piece_below(foot, cut_z + 0.01)
            if up_joined or max(np.ptp(fc[up, 0]), np.ptp(fc[up, 1])) > max(np.ptp(fc[ids, 0]), np.ptp(fc[ids, 1])) * 1.25 + 0.02:
                break
            ids, cut_z = up, cut_z + 0.01
        return ids, cut_z

    bad, bad_cut = whole_leg(spot)
    good, good_cut = whole_leg(src_foot)
    dup = bmesh.ops.duplicate(bm, geom=[faces[j] for j in good])
    for p in (g for g in dup["geom"] if isinstance(g, bmesh.types.BMVert)):
        p.co.y = -p.co.y
        p.co.z *= 1.02  # tuck the copy's top just inside the body
    bmesh.ops.reverse_faces(bm, faces=[g for g in dup["geom"] if isinstance(g, bmesh.types.BMFace)])
    gone = [faces[j] for j in bad]
    rim = {e for f in gone for e in f.edges}
    bmesh.ops.delete(bm, geom=gone, context="FACES_ONLY")
    rim = [e for e in rim if e.is_valid and len(e.link_faces) == 1]
    capped = bmesh.ops.holes_fill(bm, edges=rim, sides=0)["faces"]
    bmesh.ops.triangulate(bm, faces=capped)
    bmesh.ops.delete(bm, geom=[p for p in bm.verts if not p.link_faces], context="VERTS")
    bm.to_mesh(body.data)
    bm.free()
    body.data.update()
    v = np.array([p.co[:] for p in body.data.vertices])
    mirror_src = (float(src_foot[0]), float(src_foot[1]))
    print("mirrored the leg at", np.round(src_foot, 3), f"(up to z {good_cut:.2f}) over the one at", np.round(spot, 3),
          f"(cut at z {bad_cut:.2f}, {len(bad)} faces)")
    align()
lab = np.zeros(1, int)
print("feet slice", cut, "island sizes", sizes)
# Pairs from front to back (F, M for six legs, R), each left (+y) then right.
order = sorted(range(legs_n), key=lambda k: -centers[k][0])
legs = {}
for pair, tag in zip(range(0, legs_n, 2), {6: ["F", "M", "R"], 4: ["F", "R"], 2: ["R"]}[legs_n]):
    left, right = sorted(order[pair:pair + 2], key=lambda k: -centers[k][1])
    legs[tag + "L"], legs[tag + "R"] = centers[left], centers[right]
print("legs", {k: [round(float(c[0]), 3), round(float(c[1]), 3)] for k, c in legs.items()})

# ---- measure the body instead of assuming proportions (tortoises, giraffes and cats all differ)
if legs_n == 2:
    # Bipeds (birds): hips over the feet, the body runs forward from them; the belly is the gap between the legs.
    leg_x = (legs["RL"][0] + legs["RR"][0]) / 2
    fx, rx = leg_x + 0.22 * L, leg_x - 0.02
    mid = v[(np.abs(v[:, 0] - leg_x) < 0.12 * L) & (np.abs(v[:, 1]) < 0.04)]
else:
    fx = (legs["FL"][0] + legs["FR"][0]) / 2
    rx = (legs["RL"][0] + legs["RR"][0]) / 2
    # on the centre line only: front legs standing close together (Cinderfang's) reach into a wider band
    mid = v[(v[:, 0] > rx + 0.15 * (fx - rx)) & (v[:, 0] < fx - 0.15 * (fx - rx)) & (np.abs(v[:, 1]) < 0.06)]
belly = float(np.percentile(mid[:, 2], 3)) if len(mid) else 0.35
back = float(mid[:, 2].max()) if len(mid) else 0.8
# --belly-z 0.31: something hanging under the belly reads as the belly (Basaltback's long tusk sweeps back under it)
if "--belly-z" in args:
    belly = float(args[args.index("--belly-z") + 1])
hip_z = float(np.clip(belly + 0.45 * (back - belly), 0.2, 0.8))
# --hip-z 0.5: a sail or crest along the spine reads as the back and lifts the hips (Magmaw's sail put them at 0.65)
if "--hip-z" in args:
    hip_z = float(args[args.index("--hip-z") + 1])
knee_z = max(belly * 0.55, hip_z * 0.4)  # squat critters still get a real knee
ankle_z = max(0.04, knee_z * 0.3)
cap_f = v[v[:, 0] > hi[0] - 0.12 * L]
cap_r = v[v[:, 0] < lo[0] + 0.10 * L]
head_tip = (float(hi[0]) - 0.01, 0.0, float(cap_f[:, 2].mean()))
tail_tip = (float(lo[0]) + 0.01, 0.0, float(cap_r[:, 2].mean()))
print(f"body belly {belly:.2f} back {back:.2f} hip {hip_z:.2f} head z {head_tip[2]:.2f} tail z {tail_tip[2]:.2f}")

# ---- follow each leg up the mesh: three-quarter concepts give slanted, splayed legs, and a bone standing straight up
# from the foot ends up outside the leg (its skin then goes to a neighbouring leg's bones and stretches in every step)
import bmesh
_bm = bmesh.new()  # welded copy for following the surface (the mesh itself is split at UV seams)
_bm.from_mesh(body.data)
bmesh.ops.remove_doubles(_bm, verts=_bm.verts, dist=0.0005)
_faces = list(_bm.faces)
_fc = np.array([f.calc_center_median()[:] for f in _faces])
_ftop = np.array([max(p.co.z for p in f.verts) for f in _faces])
_index = {f: i for i, f in enumerate(_faces)}


def leg_surface(foot, cut_z):
    """Faces below cut_z joined to this foot; True if they also reach another foot (the cut took in the belly)."""
    below = _ftop < cut_z
    d = np.where(below, (_fc[:, 0] - foot[0]) ** 2 + (_fc[:, 1] - foot[1]) ** 2 + _fc[:, 2] ** 2, np.inf)
    start = int(np.argmin(d))
    seen, stack = {start}, [start]
    while stack:
        for e in _faces[stack.pop()].edges:
            for h in e.link_faces:
                j = _index[h]
                if below[j] and j not in seen:
                    seen.add(j)
                    stack.append(j)
    ids = np.fromiter(seen, int)
    others = [c for k, c in legs.items() if np.hypot(c[0] - foot[0], c[1] - foot[1]) > 0.06]
    joined = any(np.any((np.hypot(_fc[ids, 0] - o[0], _fc[ids, 1] - o[1]) < 0.06) & (_fc[ids, 2] < 0.08)) for o in others)
    return ids, joined


def trace_leg(foot):
    """Centre line of the leg standing on `foot`, from the ground up to where it merges into the body."""
    cut_z = hip_z
    ids, joined = leg_surface(foot, cut_z)
    while joined and cut_z > 0.1:
        cut_z -= 0.02
        ids, joined = leg_surface(foot, cut_z)
    pts = _fc[ids]
    line = []
    for z in np.arange(0.02, cut_z, 0.02):
        band = pts[np.abs(pts[:, 2] - z) < 0.012]
        if len(band) >= 3:
            line.append((float(band[:, 0].mean()), float(band[:, 1].mean()), float(z)))
    if len(line) < 3:  # nothing to follow: stand it straight up
        line = [(float(foot[0]), float(foot[1]), z) for z in (0.02, 0.06, 0.1)]
    return np.array(line)


def along(line, z, mid_slant=False):
    """Point on the traced line at height z (above its top, carried on along the upper part's slant, a little)."""
    if z <= line[-1, 2]:
        k = min(max(int(np.searchsorted(line[:, 2], z)), 1), len(line) - 1)
        a, b = line[k - 1], line[k]
        return a + (b - a) * ((z - a[2]) / max(1e-6, b[2] - a[2]))
    if mid_slant:
        # Front legs: not the top few points, which swing toward the chest the leg merges into (Cinderfang's shoulders
        # landed in front of its legs, so the standing legs tilted forward). Hind legs zig-zag (knee, hock), so their
        # middle slant says nothing about the hip.
        k0 = int(len(line) * 0.3)
        top = line[k0:max(int(len(line) * 0.85), k0 + 2)]
        reach = 0.2
    else:
        top = line[max(0, len(line) - 5):]
        reach = 0.1
    d = (top[-1] - top[0]) / max(1e-6, top[-1, 2] - top[0, 2])
    d[:2] = np.clip(d[:2], -1.0, 1.0)  # at most 45 degrees of slant
    p = top[-1] + d * min(z - top[-1, 2], reach)
    p[2] = z
    return p


traced = {}
for n, (x, y) in legs.items():
    line = trace_leg((x, y))
    hip = along(line, hip_z - 0.02, mid_slant=n[0] == "F")
    hip[2] = hip_z - 0.02
    near = v[(np.abs(v[:, 0] - hip[0]) < 0.05) & (np.abs(v[:, 2] - hip[2]) < 0.04)]
    inside = float(np.percentile(np.abs(near[:, 1]), 80)) * 0.7 if len(near) else 0.1
    hip[1] = float(np.clip(hip[1], -inside, inside))  # the hip joint sits inside the body
    # Front legs fold at the wrist, low on the leg (a fold halfway down the forearm reads as an extra joint); hind
    # legs at the hock. The knee sits a little to the side the leg folds to (front: apex forward, hind: apex back):
    # the animation bends it that way, and a rest bend the other way would flip through straight.
    # --front-wrist 0.11 puts it at the model's own wrist, the top of the paw (Cinderfang's long forearm folded at 0.16).
    # --front-elbow puts it at the elbow instead, under the chest, apex back like a hock: the forearm and paw then swing
    # as one straight piece (a long forearm folding at a short pastern hooked the paw back: Cinderfang).
    # --front-elbow 0.4 sets its height: pillar legs (Basaltback's elephant legs) fold up inside the body, so the leg
    # below the belly swings as one straight column (at the belly it bent like a backwards knee mid-leg).
    if n[0] == "F" and front_elbow:
        kz = front_elbow_z or belly
    else:
        kz = (front_wrist or max(ankle_z + 0.04, knee_z * 0.7)) if n[0] == "F" else knee_z
    knee, ankle = along(line, kz), along(line, ankle_z)
    # --front-hip-x 0.20: where the shoulder is, along the body, when the trace can't tell (fur hanging off the back of
    # Emberhorn's legs stopped it low and slanted it backwards: the shoulders landed behind the hooves). The joint
    # below then sits on the straight line from the ankle up to it.
    if n[0] == "F" and front_hip_x is not None:
        hip[0] = front_hip_x
        knee = ankle + (hip - ankle) * ((kz - ankle_z) / max(1e-6, hip[2] - ankle_z))
    kx = 0.015 if n[0] == "F" and not front_elbow else -0.02
    traced[n] = ((float(hip[0]), float(hip[1]), float(hip[2])), (float(knee[0]) + kx, float(knee[1]), kz),
                 (float(ankle[0]), float(ankle[1]), ankle_z))
    print(f"leg {n}: traced up to z {line[-1, 2]:.2f}, hip {np.round(hip, 3)}, foot {round(float(x), 3), round(float(y), 3)}")

# A leg that slants in under the chest can trace across the middle (Cinderfang's left front came out with its shoulder
# on the right): that hip takes its partner's, mirrored, or failing that sits just inside its own side.
for n, (x, y) in legs.items():
    hip = traced[n][0]
    if hip[1] * y < 0:
        mate = traced.get(n[:-1] + ("R" if n[-1] == "L" else "L"))
        hy = -mate[0][1] if mate and mate[0][1] * y < 0 else 0.03 * np.sign(y)
        traced[n] = ((hip[0], float(hy), hip[2]),) + traced[n][1:]
        print(f"leg {n}: hip crossed the middle, moved to y {hy:+.3f}")

# Both hips of a pair sit in one pelvis (or one pair of shoulders): a leg modelled mid-stride slants its trace and puts
# its hip far ahead of or behind its partner's (Rimeclaw's hind hips came out 14 cm apart). The pair shares their mean.
for pre in ("F", "M", "R"):
    pair = [n for n in (pre + "L", pre + "R") if n in traced]
    if len(pair) == 2:
        hx = [traced[n][0][0] for n in pair]
        if abs(hx[0] - hx[1]) > 0.04:
            for n in pair:
                traced[n] = ((float(np.mean(hx)), traced[n][0][1], traced[n][0][2]),) + traced[n][1:]
            print(f"legs {pre}: hips {hx[0]:+.3f} / {hx[1]:+.3f} along the body, both set to {np.mean(hx):+.3f}")

# A mirrored leg gets its source leg's bones, mirrored (its own trace stops at the copy's top, the source's runs on
# into the body: the pair would stand lopsided).
if "--mirror-leg" in args:
    near = lambda x, y: min(legs, key=lambda k: np.hypot(legs[k][0] - x, legs[k][1] - y))
    src_n, dst_n = near(*mirror_src), near(mirror_src[0], -mirror_src[1])
    if src_n != dst_n:
        traced[dst_n] = tuple((p[0], -p[1], p[2]) for p in traced[src_n])
        print(f"leg {dst_n}: bones mirrored from {src_n}")

# ---- armature
arm_data = bpy.data.armatures.new("Rig")
rig = bpy.data.objects.new("Rig", arm_data)
bpy.context.scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="EDIT")
eb = arm_data.edit_bones

def bone(name, head, tail, parent=None, roll_to=(0, 0, 1)):
    b = eb.new(name)
    b.head, b.tail = Vector(head), Vector(tail)
    b.align_roll(Vector(roll_to))
    if parent:
        b.parent = eb[parent]
    return b

front_x, rear_x = fx, rx
bone("root", (0, 0, 0), (0, 0, 0.2))  # points up: its local Y is world up, so lift with location.y
bone("pelvis", (rear_x, 0, hip_z), (rear_x + (front_x - rear_x) * 0.25, 0, hip_z + 0.02), "root")
spine_pts = [rear_x + (front_x - rear_x) * f for f in (0.25, 0.55, 0.85, 1.05)]
prev = "pelvis"
for i in range(3):
    bone(f"spine{i + 1}", (spine_pts[i], 0, hip_z + 0.02), (spine_pts[i + 1], 0, hip_z + 0.04), prev)
    prev = f"spine{i + 1}"
neck_start = eb["spine3"].tail
neck_end = (front_x + (head_tip[0] - front_x) * 0.55, 0, (neck_start.z + head_tip[2]) / 2 + 0.08 * abs(head_tip[2] - neck_start.z))
bone("neck", neck_start, neck_end, "spine3")
bone("head", neck_end, head_tip, "neck")
tail_mid = (rear_x - (rear_x - tail_tip[0]) * 0.5, 0, (hip_z + tail_tip[2]) / 2)
bone("tail1", (rear_x, 0, hip_z), tail_mid, "pelvis")
bone("tail2", tail_mid, tail_tip, "tail1")
for n in legs:
    par = {"F": "spine3", "M": "spine1", "R": "pelvis"}[n[0]]
    side = (0, 1, 0)  # leg bones swing about their local Z (= world lateral axis)
    hip, knee, ankle = traced[n]
    bone(f"{n}_upper", hip, knee, par, side)
    bone(f"{n}_lower", knee, ankle, f"{n}_upper", side)
    bone(f"{n}_foot", ankle, (ankle[0] + 0.06, ankle[1], 0.0), f"{n}_lower", side)
bpy.ops.object.mode_set(mode="OBJECT")

# ---- skin
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
# Textured image-to-3D meshes are split along UV seams, which defeats automatic weights: weld first
# (UVs live on face corners, so the texture survives), and fall back to envelope weights if it still fails.
bpy.context.view_layer.objects.active = body
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.remove_doubles(threshold=0.0005)
# Automatic weights fail outright on a mesh with many loose pieces (Cinderfang's tail is a core and 130 fur strands):
# weight the biggest piece, the body, on its own, then join the rest back (they get nearest bones below).
bpy.ops.mesh.separate(type="LOOSE")
bpy.ops.object.mode_set(mode="OBJECT")
body_name = body.name
pieces = [o for o in bpy.context.scene.objects if o.type == "MESH"]
body = max(pieces, key=lambda o: len(o.data.vertices))
loose = [o for o in pieces if o is not body]
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_set(type="ARMATURE_AUTO")
if loose:
    bpy.ops.object.select_all(action="DESELECT")
    for o in loose:
        o.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()
    print("loose pieces joined back:", len(loose))
body.name = body_name
unweighted = sum(1 for p in body.data.vertices if not p.groups)
if unweighted > 0.5 * len(body.data.vertices):
    print("auto weights failed for", unweighted, "verts; using envelopes")
    for vg in list(body.vertex_groups):
        body.vertex_groups.remove(vg)
    for b in arm_data.bones:
        b.envelope_distance = 0.12
        b.head_radius = b.tail_radius = 0.06
    bpy.ops.object.parent_set(type="ARMATURE_ENVELOPE")
    unweighted = sum(1 for p in body.data.vertices if not p.groups)
# Automatic weights skip pieces not joined to the body (fur strands, a horn, a tail built separately): each of their
# vertices goes to its two nearest bones instead.
if unweighted:
    segs = [(b.name, np.array(b.head_local[:]), np.array(b.tail_local[:])) for b in arm_data.bones if b.use_deform]
    groups = {vg.name: vg for vg in body.vertex_groups}
    for p in body.data.vertices:
        if p.groups:
            continue
        c = np.array(p.co[:])
        d = []
        for name, a, b in segs:
            ab = b - a
            t = float(np.clip(np.dot(c - a, ab) / max(1e-9, np.dot(ab, ab)), 0.0, 1.0))
            d.append((float(np.linalg.norm(c - (a + ab * t))), name))
        d.sort()
        w = [1.0 / max(1e-4, dist) ** 4 for dist, _ in d[:2]]
        for (dist, name), wi in zip(d[:2], w):
            groups.setdefault(name, body.vertex_groups.new(name=name)).add([p.index], wi / sum(w), "REPLACE")
    print("loose pieces:", unweighted, "verts weighted to their nearest bones")
    unweighted = sum(1 for p in body.data.vertices if not p.groups)
print("unweighted verts", unweighted, "of", len(body.data.vertices))

# ---- animations
FPS = 30
bpy.context.scene.render.fps = FPS
rig.animation_data_create()
for pb in rig.pose.bones:
    pb.rotation_mode = "XYZ"

def make_action(name, seconds, fn, loop=True):
    act = bpy.data.actions.new(name)
    rig.animation_data.action = act
    frames = int(seconds * FPS)
    for f in range(frames + 1):
        t = f / frames
        for pb in rig.pose.bones:
            pb.rotation_euler = (0, 0, 0)
            pb.location = (0, 0, 0)
        fn(t)
        for pb in rig.pose.bones:
            pb.keyframe_insert("rotation_euler", frame=f + 1)
            if pb.name == "root":
                pb.keyframe_insert("location", frame=f + 1)
                pb.keyframe_insert("rotation_euler", frame=f + 1)
    act.use_fake_user = True
    return act

P = rig.pose.bones
R = math.radians
phase = {"FL": 0.0, "RR": 0.0, "FR": 0.5, "RL": 0.5}  # trot: diagonal pairs
if gait_kind == "hop":
    phase = {"FL": 0.0, "FR": 0.0, "RL": 0.5, "RR": 0.5}  # hop: front pair lands, rear pair pushes off
if legs_n == 6:
    phase = {"FL": 0.0, "MR": 0.0, "RL": 0.0, "FR": 0.5, "ML": 0.5, "RR": 0.5}  # tripod: three feet always down
if legs_n == 2:
    phase = {"RL": 0.0, "RR": 0.5}  # biped: alternate, with a waddle (below)

def gait(t, swing, knee, bob):
    for n, ph in phase.items():
        a = 2 * math.pi * (t + ph)
        P[f"{n}_upper"].rotation_euler.z = R(swing) * math.sin(a)
        P[f"{n}_lower"].rotation_euler.z = -R(knee) * max(0.0, math.sin(a + 1.2))
        P[f"{n}_foot"].rotation_euler.z = R(knee * 0.5) * max(0.0, math.sin(a + 1.6))
    P["root"].location.y = bob * abs(math.sin(2 * math.pi * t * 2))
    if legs_n == 2:
        P["root"].rotation_euler.x = R(4) * math.sin(2 * math.pi * t)  # waddle onto each planted foot
    P["head"].rotation_euler.x = R(3) * math.sin(4 * math.pi * t)
    P["tail1"].rotation_euler.z = R(10) * math.sin(2 * math.pi * t)

make_action("Idle", 2.0, lambda t: (setattr(P["spine2"].rotation_euler, "x", R(1.5) * math.sin(2 * math.pi * t)),
                                     setattr(P["head"].rotation_euler, "x", R(4) * math.sin(2 * math.pi * t + 1)),
                                     setattr(P["tail1"].rotation_euler, "z", R(8) * math.sin(4 * math.pi * t))))
make_action("Walk", 1.0, lambda t: gait(t, 22, 25, 0.06 if gait_kind == "hop" else 0.01))
make_action("Run", 0.55, lambda t: gait(t, 36, 40, 0.12 if gait_kind == "hop" else 0.035))

def attack(t):
    k = math.sin(math.pi * min(1.0, t / 0.4)) if t < 0.4 else 0.0     # wind down
    j = math.sin(math.pi * (t - 0.35) / 0.35) if 0.35 < t < 0.7 else 0.0  # lunge
    P["neck"].rotation_euler.x = R(-18) * k + R(10) * j
    P["head"].rotation_euler.x = R(-20) * k + R(18) * j
    P["spine3"].rotation_euler.x = R(-6) * k
    P["root"].location.x = 0.12 * j
make_action("Attack", 0.8, attack, loop=False)

def hit(t):
    k = math.sin(math.pi * t)
    P["spine2"].rotation_euler.x = R(8) * k
    P["head"].rotation_euler.x = R(12) * k
    P["root"].location.x = -0.04 * k
make_action("Hit", 0.3, hit, loop=False)

def death(t):
    e = min(1.0, t / 0.7) ** 2
    P["root"].rotation_euler.x = R(85) * e
    P["root"].location.y = -0.05 * e
    for n in phase:
        P[f"{n}_upper"].rotation_euler.z = R(15) * e * (1 if n[0] == "F" else -1)
    P["neck"].rotation_euler.x = R(-10) * e
make_action("Death", 1.0, death, loop=False)

# ---- optional preview: walk at mid-stride
if preview:
    rig.animation_data.action = bpy.data.actions["Walk"]
    bpy.context.scene.frame_set(8)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.show_cavity = True
    scene.display.shading.color_type = "TEXTURE" if body.data.materials else "MATERIAL"
    scene.render.resolution_x, scene.render.resolution_y = 800, 600
    scene.world = bpy.data.worlds.new("w")
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    for i, loc in enumerate([(0.0, -2.8, 0.6), (2.2, -1.6, 1.0)]):  # side, front three-quarter
        cam.location = loc
        cam.rotation_euler = (Vector((0, 0, 0.45)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = preview.replace(".png", f"_{i}.png")
        bpy.ops.render.render(write_still=True)

# Unreal's importer can't read textures embedded in the FBX: write them next to it as PNGs instead.
import os
for img in bpy.data.images:
    if img.packed_file or not img.filepath:
        img.filepath_raw = os.path.join(os.path.dirname(os.path.abspath(out)), f"{os.path.splitext(os.path.basename(out))[0]}_{img.name}.png".replace(" ", "_"))
        img.file_format = "PNG"
        img.save()
        if img.packed_file:
            img.unpack(method="REMOVE")
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                         bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                         mesh_smooth_type="FACE", path_mode="ABSOLUTE", embed_textures=False, axis_forward="X", axis_up="Z")
print("exported", out, "actions", [a.name for a in bpy.data.actions])
