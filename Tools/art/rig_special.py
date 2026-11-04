# Blender (headless): rig image-to-3D creatures that aren't legged walkers, bake their animations, export FBX for Unreal.
#   blender -b -P rig_special.py -- <model.glb> <out.fbx> --body worm|moth|fae [--head +x|-x|+y|-y] [--hover 1.2] [--stretch 1.6]
#       [--wing wing.png [--wing-root left|right] [--wing-span 0.5]] [--preview out.png]
#   worm  a 10-bone spine along the body's centreline: slithers like a snake (waves running back down a body lying
#         flat, the head held steady), rears its front half up and strikes. --head-info <npz> (gl_build.py writes one
#         for Glacimaw: neck_x, and the lower jaw as head_pos / jaw_w / hinge / chin, in the model's own units) gives
#         it a rigid head bone from the neck and a jaw bone that opens as it rears and snaps shut on the strike.
#   moth  the concept is seen from above, so the model stands on its tail: it's laid flat (head forward, back up),
#         then gets a body, a head and one hinged wing per side; it hovers off the ground and flaps.
#   fae   an upright winged figure seen from the front (Ignessa): faced the camera, so it's turned to face +X; a body,
#         a head and a hinged wing per side behind the shoulders. It floats, sways and fans its wings (the game flies it).
#         --wing builds both wings from a picture of one (for a model made without wings: image models weld them to arms),
#         rooted in her back at the shoulder blades, and gives her arms and legs (thigh/shin/foot, upperarm/forearm/hand):
#         legs hang soft with pointed toes and trail behind in flight, hands draw back and thrust in the attack.
#         --crotch 0.47 (height where the legs meet).
# Actions: Idle, Walk, Run, Attack, Hit, Death (30 fps), same names as rig_quadruped.py so the game wires them alike.
import bpy, sys, math, os
import numpy as np
from mathutils import Vector, Matrix

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
kind = args[args.index("--body") + 1]
head_dir = args[args.index("--head") + 1] if "--head" in args else "+x"
preview = args[args.index("--preview") + 1] if "--preview" in args else None
hover = float(args[args.index("--hover") + 1]) if "--hover" in args else 1.2  # moth: body heights off the ground
# fae: wings built from a picture of one wing instead of found in the model (image models weld wings to arms and legs)
wing_png = args[args.index("--wing") + 1] if "--wing" in args else None
wing_root = args[args.index("--wing-root") + 1] if "--wing-root" in args else "left"  # the picture's edge that attaches
wing_span = float(args[args.index("--wing-span") + 1]) if "--wing-span" in args else 0.5  # each wing, in body heights

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
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
if kind == "fae":
    body.data.transform(Matrix.Rotation(math.pi / 2, 4, "Z"))  # faced the camera (-Y): now faces +X
if kind == "moth":
    # Image top (+Z) becomes forward (+X); the side that faced the camera (-Y) becomes the back (+Z).
    body.data.transform(Matrix(((0, 0, 1, 0), (-1, 0, 0, 0), (0, -1, 0, 0), (0, 0, 0, 1))))
yaw = {"+x": 0, "-x": math.pi, "+y": -math.pi / 2, "-y": math.pi / 2}[head_dir]
body.data.transform(Matrix.Rotation(yaw, 4, "Z"))
v = np.array([p.co[:] for p in body.data.vertices])
lo, hi = v.min(0), v.max(0)
body.data.transform(Matrix.Translation(Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2]))))
body.data.transform(Matrix.Scale(1.0 / (hi[2] - lo[2]), 4))  # 1 unit tall, like the other rigs
stretch = float(args[args.index("--stretch") + 1]) if "--stretch" in args else 1.0
if "--stretch" in args:  # a longer body (a worm), its thickness kept
    body.data.transform(Matrix.Scale(stretch, 4, Vector((1, 0, 0))))


def to_rig(p, _yaw=Matrix.Rotation(yaw, 3, "Z"), _t=Vector((-(lo[0] + hi[0]) / 2, -(lo[1] + hi[1]) / 2, -lo[2])), _s=1.0 / (hi[2] - lo[2])):
    """a point in the model's own units (as --head-info gives them) where it lands in the rig's"""
    q = (_yaw @ Vector(p) + _t) * _s
    q.x *= stretch
    return q
body.location = (0, 0, 0)
bpy.ops.object.shade_smooth()
v = np.array([p.co[:] for p in body.data.vertices])
lo, hi = v.min(0), v.max(0)
L, W = hi[0] - lo[0], hi[1] - lo[1]
print(f"{kind}: length {L:.2f} width {W:.2f} (height 1)")

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

bone("root", (0, 0, 0), (0, 0, 0.2))  # points up: its local Y is world up, so lift with location.y
SEGS = 10
head_info = np.load(args[args.index("--head-info") + 1]) if "--head-info" in args else None
if kind == "worm":
    # Joints follow the body's centreline, tail (-X) to head (+X): to the neck when there's a head bone after it. With
    # a head bone the joints sit at each slice's median height (the back spikes don't lift them off the centreline).
    neck = to_rig((float(head_info["neck_x"]), 0, 0)).x if head_info is not None else hi[0] - 0.01 * L
    xs = np.linspace(lo[0] + 0.02 * L, neck, SEGS + 1)
    joints = []
    for x in xs:
        sl = v[np.abs(v[:, 0] - x) < L / (2 * SEGS)]
        if not len(sl):
            joints.append((x, 0.0, 0.5))
            continue
        mid = float(np.median(sl[:, 2])) if head_info is not None else float((sl[:, 2].min() + sl[:, 2].max()) / 2)
        joints.append((x, float(np.median(sl[:, 1])), mid))
    prev = "root"
    for i in range(SEGS):
        bone(f"spine{i + 1}", joints[i], joints[i + 1], prev)
        prev = f"spine{i + 1}"
    if head_info is not None:
        bone("head", joints[-1], (hi[0] - 0.01 * L, joints[-1][1], joints[-1][2]), prev)  # level, to the snout
        if "jaw_w" in head_info:
            bone("jaw", to_rig(head_info["hinge"]), to_rig(head_info["chin"]), "head")
    print("centreline", [[round(c, 2) for c in j] for j in joints])
elif kind == "fae" and wing_png:
    # Built wings grow out of her back opposite her breasts (Raven: they hung off the back of her hair at the neck, then
    # still looked joined at her head from the shoulders): at the height where her chest stands farthest forward
    # (--wing-z to set it), either side of the spine, each wing's root just inside the back there. The neck is the
    # narrowest slice of the upper body (the arms hang from just under it).
    slices = [(z, float(np.percentile(np.abs(v[np.abs(v[:, 2] - z) < 0.006][:, 1]), 90)))
              for z in np.arange(0.62, 0.92, 0.01) if np.count_nonzero(np.abs(v[:, 2] - z) < 0.006) > 20]
    neck_z = min(slices, key=lambda sl: sl[1])[0] if slices else 0.8
    chest = v[(v[:, 2] > 0.55) & (v[:, 2] < neck_z - 0.04) & (np.abs(v[:, 1]) < 0.06)]
    wz = float(chest[np.argmax(chest[:, 0]), 2]) if len(chest) else neck_z - 0.18
    if "--wing-z" in args:
        wz = float(args[args.index("--wing-z") + 1])
    wy = 0.05
    band = v[(np.abs(v[:, 2] - wz) < 0.02) & (np.abs(np.abs(v[:, 1]) - wy) < 0.012)]
    wx = float(band[:, 0].min()) + 0.015 if len(band) else lo[0]
    # Arms and legs, so they float and reach instead of hanging stiff off the body. Each leg's joints sit in its middle
    # at the hip (just under the crotch), knee and ankle heights, the foot to the toes; an arm hangs clear of the body
    # below the elbow (a gap in each slice going up from the hand), the hand the farthest-out point below that.
    crotch = float(args[args.index("--crotch") + 1]) if "--crotch" in args else 0.47
    J = {}
    for s in (1, -1):
        side = v[v[:, 1] * s > 0]
        legs = side[(side[:, 2] < crotch) & (np.abs(side[:, 1]) < 0.125)]

        def mid(z, band=0.012):  # the middle of the leg's outline (strands at the crotch pull a median inward)
            q = legs[np.abs(legs[:, 2] - z) < band]
            return np.array([(q[:, 0].min() + q[:, 0].max()) / 2, (q[:, 1].min() + q[:, 1].max()) / 2, z])
        hip, knee, ankle = mid(crotch - 0.08, 0.008), mid(0.53 * crotch), mid(0.1 * crotch)
        hip[2] = crotch - 0.01
        feet = legs[legs[:, 2] < 0.03]
        toe = np.array([feet[:, 0].max() - 0.005, ankle[1], 0.015])
        elbow = None
        for z in np.arange(0.45, 0.75, 0.005):
            q = side[np.abs(side[:, 2] - z) < 0.004]
            ys = np.sort(np.abs(q[:, 1]))
            gaps = np.nonzero((np.diff(ys) > 0.012) & (ys[:-1] > 0.05))[0] if len(ys) > 1 else []
            if not len(gaps):
                break
            arm = q[np.abs(q[:, 1]) >= ys[gaps[0] + 1]]
            elbow = np.array([arm[:, 0].mean(), s * np.abs(arm[:, 1]).mean(), z + 0.01])
        if elbow is None:
            elbow = np.array([0.0, s * 0.13, 0.6])
        zone = side[(side[:, 2] < elbow[2]) & (side[:, 2] > 0.3)]
        tip = zone[np.argmax(np.abs(zone[:, 1]))]
        wrist = elbow + 0.58 * (tip - elbow)
        q = side[np.abs(side[:, 2] - (neck_z - 0.08)) < 0.01]
        shoulder = np.array([np.median(q[:, 0]), s * 0.8 * np.abs(q[:, 1]).max(), neck_z - 0.08])
        J[s] = dict(hip=hip, knee=knee, ankle=ankle, toe=toe, shoulder=shoulder, elbow=elbow, wrist=wrist, tip=tip)
    # from the hips (the legs have their own bones now); rolled to face forward, so an X turn leans her forward and
    # back (an upright bone left unrolled turned X sideways: the lunge, flight lean and fall tipped her over sideways)
    bone("body", (0, 0, crotch - 0.02), (0, 0, 0.8), "root", roll_to=(1, 0, 0))
    bone("head", (0, 0, 0.8), (0, 0, 0.99), "body", roll_to=(1, 0, 0))
    bone("wing_L", (wx, wy, wz), (wx - 0.08, wy + wing_span * 0.9, wz + 0.15), "body")
    bone("wing_R", (wx, -wy, wz), (wx - 0.08, -wy - wing_span * 0.9, wz + 0.15), "body")
    for s, sfx in ((1, "L"), (-1, "R")):
        j = {k: tuple(float(c) for c in p) for k, p in J[s].items()}
        # limbs roll forward: +X turns swing each one forward (an elbow bends, a knee goes the other way)
        bone(f"thigh_{sfx}", j["hip"], j["knee"], "body", roll_to=(1, 0, 0))
        bone(f"shin_{sfx}", j["knee"], j["ankle"], f"thigh_{sfx}", roll_to=(1, 0, 0))
        bone(f"foot_{sfx}", j["ankle"], j["toe"], f"shin_{sfx}", roll_to=(0, 0, 1))
        bone(f"upperarm_{sfx}", j["shoulder"], j["elbow"], "body", roll_to=(1, 0, 0))
        bone(f"forearm_{sfx}", j["elbow"], j["wrist"], f"upperarm_{sfx}", roll_to=(1, 0, 0))
        bone(f"hand_{sfx}", j["wrist"], j["tip"], f"forearm_{sfx}", roll_to=(1, 0, 0))
        print(f"fae {sfx}: " + "  ".join(f"{k} {np.round(p, 3).tolist()}" for k, p in J[s].items()))
    print(f"fae: neck at z {neck_z:.2f}, wings grow from x {wx:.3f} y +-{wy} z {wz:.2f}")
elif kind == "fae":
    # Wings: whatever spreads well out to the sides above the waist; hinge them behind the shoulders, at their height.
    wing = v[(np.abs(v[:, 1]) > 0.28 * W) & (v[:, 2] > 0.35)]
    wx = float(np.median(wing[:, 0])) if len(wing) else lo[0] * 0.5
    wz = float(np.median(wing[:, 2])) if len(wing) else 0.7
    bone("body", (0, 0, 0.05), (0, 0, 0.8), "root")
    bone("head", (0, 0, 0.8), (0, 0, 0.99), "body")
    bone("wing_L", (wx, 0.06 * W, wz), (wx, 0.46 * W, wz + 0.05), "body")
    bone("wing_R", (wx, -0.06 * W, wz), (wx, -0.46 * W, wz + 0.05), "body")
    print(f"fae wings hinge at x {wx:.2f} z {wz:.2f} ({len(wing)} wing verts)")
else:
    core = v[np.abs(v[:, 1]) < 0.06 * W]
    zc = float(np.median(core[:, 2])) if len(core) else 0.5
    wing = v[np.abs(v[:, 1]) > 0.3 * W]
    wx = float(wing[:, 0].mean()) if len(wing) else 0.0
    neck = hi[0] - 0.22 * L
    bone("body", (lo[0] + 0.05 * L, 0, zc), (neck, 0, zc), "root")
    bone("head", (neck, 0, zc), (hi[0] - 0.01 * L, 0, zc), "body")
    root_y = 0.07 * W
    bone("wing_L", (wx, root_y, zc), (wx, 0.48 * W, zc), "body")
    bone("wing_R", (wx, -root_y, zc), (wx, -0.48 * W, zc), "body")
bpy.ops.object.mode_set(mode="OBJECT")

# ---- skin (weld UV-seam splits first, as in rig_quadruped.py)
bpy.context.view_layer.objects.active = body
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.remove_doubles(threshold=0.0005)
bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.object.select_all(action="DESELECT")
body.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.parent_set(type="ARMATURE_AUTO")
unweighted = sum(1 for p in body.data.vertices if not p.groups)
if unweighted > 0.1 * len(body.data.vertices):
    print("auto weights failed for", unweighted, "verts; using envelopes")
    for vg in list(body.vertex_groups):
        body.vertex_groups.remove(vg)
    for b in arm_data.bones:
        b.envelope_distance = 0.25 * max(1.0, L / 3)
        b.head_radius = b.tail_radius = 0.12
    bpy.ops.object.parent_set(type="ARMATURE_ENVELOPE")
    unweighted = sum(1 for p in body.data.vertices if not p.groups)
if unweighted:
    # Loose pieces automatic weights skipped (Glacimaw's crystals): each vertex takes its nearest weighted vertex's.
    from mathutils.kdtree import KDTree
    weighted = [p for p in body.data.vertices if p.groups]
    kd = KDTree(len(weighted))
    for k, p in enumerate(weighted):
        kd.insert(p.co, k)
    kd.balance()
    for p in [p for p in body.data.vertices if not p.groups]:
        for g in [(g.group, g.weight) for g in weighted[kd.find(p.co)[1]].groups]:
            body.vertex_groups[g[0]].add([p.index], g[1], "REPLACE")
    print("loose pieces:", unweighted, "verts weighted like their nearest skin")
    unweighted = sum(1 for p in body.data.vertices if not p.groups)
print("unweighted verts", unweighted, "of", len(body.data.vertices))
if head_info is not None and "jaw_w" in head_info:
    # The jaw takes exactly the weights --head-info gives (heat weights can't tell the lower lip from the upper teeth
    # hanging past it); everything else keeps its own, scaled to what the jaw leaves.
    from mathutils.kdtree import KDTree
    kd = KDTree(len(head_info["head_pos"]))
    for k, p in enumerate(head_info["head_pos"]):
        kd.insert(to_rig(p), k)
    kd.balance()
    jaw_g = body.vertex_groups.get("jaw") or body.vertex_groups.new(name="jaw")
    on_jaw = 0
    for p in body.data.vertices:
        _, k, dist = kd.find(p.co)
        w = float(head_info["jaw_w"][k]) if dist < 1e-3 else 0.0
        rest_w = [(g.group, g.weight) for g in p.groups if g.group != jaw_g.index]
        total = sum(x for _, x in rest_w)
        for gi, x in rest_w:
            body.vertex_groups[gi].add([p.index], (x / total if total > 0 else 0.0) * (1 - w), "REPLACE")
        if w > 0:
            jaw_g.add([p.index], w, "REPLACE")
            on_jaw += w > 0.5
        else:
            jaw_g.remove([p.index])
    print("jaw:", on_jaw, "verts on the jaw bone")


def build_wings(png, root_side, span, x0, z0, y0=0.03):
    """Two wings from one wing picture on a plain background: a mesh over the picture's shape (128 cells long), UVs
    from the picture, faces both ways (seen from behind and in front), hinged at (x0, +-y0, z0) and sweeping out, up
    and back. Weighted wholly to wing_L / wing_R, so they never pull at the arms."""
    img = bpy.data.images.load(png)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)  # rows bottom-up
    rgb = px[..., :3]
    bg = np.median(np.concatenate([rgb[0], rgb[-1], rgb[:, 0], rgb[:, -1]]), 0)
    cs = max(2, max(w, h) // 128)  # square cells, 128 along the picture's long side
    NR, NC = h // cs, w // cs
    solid = np.linalg.norm(rgb - bg, axis=-1) > 0.15
    cell = solid[: NR * cs, : NC * cs].reshape(NR, cs, NC, cs).mean((1, 3)) > 0.85
    # a cell's worth in from the edge: the outline's cells carry the background's grey
    inner = cell.copy()
    inner[1:] &= cell[:-1]
    inner[:-1] &= cell[1:]
    inner[:, 1:] &= cell[:, :-1]
    inner[:, :-1] &= cell[:, 1:]
    cell = inner
    # the biggest piece only (stray sparks drop out)
    seen = np.zeros_like(cell)
    best = []
    for r0, c0 in zip(*np.nonzero(cell)):
        if seen[r0, c0]:
            continue
        seen[r0, c0] = True
        stack, piece = [(r0, c0)], []
        while stack:
            r, c = stack.pop()
            piece.append((r, c))
            for rr, cc in ((r + 1, c), (r - 1, c), (r, c + 1), (r, c - 1)):
                if 0 <= rr < NR and 0 <= cc < NC and cell[rr, cc] and not seen[rr, cc]:
                    seen[rr, cc] = True
                    stack.append((rr, cc))
        best = piece if len(piece) > len(best) else best
    cols = np.array([c for _, c in best])
    rows = np.array([r for r, _ in best])
    cmin, cmax = int(cols.min()), int(cols.max())
    scale = span / (cmax - cmin + 1)
    near = rows[np.abs(cols - (cmin if root_side == "left" else cmax)) <= 3]
    r_root = float(near.mean())
    mat = bpy.data.materials.new("Wing")
    mat.use_nodes = True
    texn = mat.node_tree.nodes.new("ShaderNodeTexImage")
    texn.image = img
    mat.node_tree.links.new(texn.outputs["Color"], mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"])
    objs = []
    for side, group in ((1, "wing_L"), (-1, "wing_R")):
        index, verts, uvs = {}, [], []

        def corner(r, c, back):
            key = (r, c, back)
            if key not in index:
                out = (c - cmin) * scale if root_side == "left" else (cmax + 1 - c) * scale
                up = (r - r_root) * scale
                index[key] = len(verts)
                verts.append((x0 - 0.45 * out - (0.004 if back else 0.0), side * (y0 + 0.85 * out), z0 + up + 0.12 * out))
                uvs.append((c * cs / w, r * cs / h))
            return index[key]

        faces = []
        for r, c in best:
            q = [corner(r, c, False), corner(r, c + 1, False), corner(r + 1, c + 1, False), corner(r + 1, c, False)]
            qb = [corner(r, c, True), corner(r + 1, c, True), corner(r + 1, c + 1, True), corner(r, c + 1, True)]
            if side < 0:
                q, qb = q[::-1], qb[::-1]
            faces += [q, qb]
        me = bpy.data.meshes.new(group)
        me.from_pydata(verts, [], faces)
        uv = me.uv_layers.new(name="UVMap")
        for poly in me.polygons:
            for li in poly.loop_indices:
                uv.data[li].uv = uvs[me.loops[li].vertex_index]
        me.materials.append(mat)
        ob = bpy.data.objects.new(group, me)
        bpy.context.scene.collection.objects.link(ob)
        vg = ob.vertex_groups.new(name=group)
        vg.add(list(range(len(verts))), 1.0, "REPLACE")
        objs.append(ob)
    print(f"built wings from {os.path.basename(png)}: {len(best)} cells each, span {span}")
    return objs


if kind == "fae" and wing_png:
    wings = build_wings(wing_png, wing_root, wing_span, wx, wz, wy)
    bpy.ops.object.select_all(action="DESELECT")
    for ob in wings:
        ob.select_set(True)
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    bpy.ops.object.join()  # one mesh, the wings a second material; the body keeps its armature modifier

# ---- animations
FPS = 30
bpy.context.scene.render.fps = FPS
rig.animation_data_create()
for pb in rig.pose.bones:
    pb.rotation_mode = "XYZ"
P = rig.pose.bones
R = math.radians

def make_action(name, seconds, fn):
    act = bpy.data.actions.new(name)
    rig.animation_data.action = act
    frames = int(seconds * FPS)
    for f in range(frames + 1):
        for pb in rig.pose.bones:
            pb.rotation_euler = (0, 0, 0)
            pb.location = (0, 0, 0)
        fn(f / frames)
        for pb in rig.pose.bones:
            pb.keyframe_insert("rotation_euler", frame=f + 1)
            if pb.name in ("root", "spine1"):  # the worm places its body by its first spine bone
                pb.keyframe_insert("location", frame=f + 1)
    act.use_fake_user = True

if kind == "worm":
    SP = [f"spine{i + 1}" for i in range(SEGS)]
    REST = {b.name: b.matrix_local.copy() for b in arm_data.bones}
    seg = [arm_data.bones[n].length for n in SP]
    LS = sum(seg)
    S_MID = [sum(seg[i + 1:]) + seg[i] / 2 for i in range(SEGS)]  # each spine bone's middle, back from the neck
    HEAD, JAW = P.get("head"), P.get("jaw")
    LAM = 0.85 * LS  # the slither's wavelength: a little over one wave down the body
    NECK0 = REST[SP[-1]] @ Vector((0, seg[-1], 0))

    def fk(name):
        """a bone's armature-space pose from its channels as set (no scene update: that would replay the action
        being keyed over them)"""
        b = arm_data.bones[name]
        m = REST[name] @ P[name].matrix_basis
        return fk(b.parent.name) @ REST[b.parent.name].inverted() @ m if b.parent else m

    def ease(x):
        x = min(1.0, max(0.0, x))
        return x * x * (3 - 2 * x)

    def pose(yaw, pitch, head_yaw=0.0, head_pitch=0.0, jaw=0.0, neck_at=None, slide=0.0):
        """Each spine bone's own heading and pitch off its rest (radians; yaw toward +Y, pitch nose up), the head's
        likewise, the jaw's (- opens). neck_at: where the neck goes (an offset from its rest) and the body follows;
        otherwise the tail stays where it lies and the whole body slides forward by `slide`."""
        prev_y = prev_p = 0.0
        for n, y_, p_ in zip(SP, yaw, pitch):
            P[n].rotation_euler = (p_ - prev_p, 0.0, y_ - prev_y)
            prev_y, prev_p = y_, p_
        if HEAD:
            HEAD.rotation_euler = (head_pitch - prev_p, 0.0, head_yaw - prev_y)
        if JAW:
            JAW.rotation_euler.x = jaw
        off = Vector((slide, 0.0, 0.0)) if neck_at is None else NECK0 + Vector(neck_at) - fk(SP[-1]) @ Vector((0, seg[-1], 0))
        P[SP[0]].location = REST[SP[0]].to_3x3().inverted() @ off

    def slither(t, amp):
        """Lateral undulation: y = A sin(2 pi (s / LAM - t)) down the body, s back from the neck, so each wave runs back
        toward the tail as the body goes forward through it; the neck sways a third as much and the head mostly keeps
        looking ahead."""
        yaw = []
        for s in S_MID:
            a = amp * (0.3 + 0.7 * ease(s / (0.35 * LS)))
            yaw.append(math.atan(-a * 2 * math.pi / LAM * math.cos(2 * math.pi * (s / LAM - t))))
        pose(yaw, [0.0] * SEGS, head_yaw=0.3 * yaw[-1], head_pitch=R(2) * math.sin(4 * math.pi * t),
             neck_at=(0.0, amp * 0.3 * math.sin(-2 * math.pi * t), 0.0))

    def idle(t):
        # lying still: a slow sway down the body, the tail tip flicking, the head looking about, the jaw breathing
        yaw = [R(1.5) * math.sin(2 * math.pi * s / LAM) * math.sin(2 * math.pi * t) + R(10) * ease((s - 0.8 * LS) / (0.2 * LS))
               * math.sin(4 * math.pi * t) for s in S_MID]
        pose(yaw, [0.0] * SEGS, head_yaw=R(6) * math.sin(2 * math.pi * t + 1.0), head_pitch=R(3) * math.sin(2 * math.pi * t),
             jaw=-R(4) * (0.5 - 0.5 * math.cos(2 * math.pi * t)), neck_at=(0.0, 0.0, 0.0))
    make_action("Idle", 4.0, idle)
    make_action("Walk", 2.6, lambda t: slither(t, 0.24))
    make_action("Run", 1.7, lambda t: slither(t, 0.30))
    print(f"worm slither: wavelength {LAM:.2f} rig units; walk 2.6 s, run 1.7 s a wave (pace = wavelength x cm per unit / seconds)")

    # The bite (Raven): it picks its front half up off the ground, the jaw gaping, coils back, then strikes forward and
    # down and chomps shut. Pitches of spine bones 6-10 (the front half), the head's, the jaw's, and the slide forward.
    REAR = dict(p={5: 18, 6: 40, 7: 58, 8: 52, 9: 12}, head=-28, jaw=-30, slide=-0.12)
    COIL = dict(p={5: 20, 6: 44, 7: 62, 8: 58, 9: 18}, head=-32, jaw=-36, slide=-0.18)
    STRIKE = dict(p={5: 10, 6: 18, 7: 10, 8: -8, 9: -16}, head=-6, jaw=7, slide=0.35)
    FLAT = dict(p={}, head=0, jaw=0, slide=0.0)

    def between(a, b, u, jaw_u=None, head_yaw=0.0):
        mix = lambda x, y, k: x + (y - x) * k
        pose([0.0] * SEGS, [R(mix(a["p"].get(i, 0), b["p"].get(i, 0), u)) for i in range(SEGS)], head_yaw=head_yaw,
             head_pitch=R(mix(a["head"], b["head"], u)), jaw=R(mix(a["jaw"], b["jaw"], u if jaw_u is None else jaw_u)),
             slide=mix(a["slide"], b["slide"], u))

    ATTACK_S = 1.5  # strike (head furthest forward) at 0.8 s: creatures.json strikeAt 0.53

    def attack(t):
        s = t * ATTACK_S
        if s < 0.5:
            between(FLAT, REAR, 1 - (1 - s / 0.5) ** 2)  # rises fast, slowing at the top
        elif s < 0.62:
            between(REAR, COIL, ease((s - 0.5) / 0.12))  # coils back
        elif s < 0.8:
            u = (s - 0.62) / 0.18
            between(COIL, STRIKE, u * u, jaw_u=ease((u - 0.6) / 0.4))  # strikes, faster and faster; the jaw snaps shut last
        elif s < 0.95:
            between(STRIKE, STRIKE, 0.0, head_yaw=R(6) * math.sin(2 * math.pi * (s - 0.8) / 0.1))  # a chomping shake
        else:
            between(STRIKE, FLAT, ease((s - 0.95) / (ATTACK_S - 0.95)))  # back down flat
    make_action("Attack", ATTACK_S, attack)

    def hit(t):
        k = math.sin(math.pi * t)
        pose([R(3) * max(0, i - 5) * k for i in range(SEGS)], [0.0] * SEGS, head_yaw=R(14) * k, head_pitch=R(10) * k, jaw=-R(15) * k)
    make_action("Hit", 0.35, hit)

    def death(t):
        e = min(1.0, t / 0.8) ** 2
        pose([R(12) * (i + 1) * e for i in range(SEGS)], [0.0] * SEGS, head_yaw=R(12) * (SEGS + 1) * e, head_pitch=-R(6) * e,
             jaw=-R(24) * e)  # curls up, the jaw falling open
    make_action("Death", 1.2, death)
elif kind == "fae":
    LIFT = 0.04  # the game flies her; just a little float of her own

    def fan(t, cycles, amp, bob, sway=0.0, pitch=0.0):
        a = R(amp) * math.sin(2 * math.pi * cycles * t)
        P["wing_L"].rotation_euler.z = a   # the wings fan forward and back about the vertical
        P["wing_R"].rotation_euler.z = -a
        P["root"].location.y = LIFT + bob * math.sin(2 * math.pi * cycles * t + 1.6)
        P["body"].rotation_euler.y = R(sway) * math.sin(2 * math.pi * t)
        P["body"].rotation_euler.x = R(pitch)
        P["head"].rotation_euler.y = -R(sway) * 0.5 * math.sin(2 * math.pi * t)

    def limbs(t, legs=(4, 12), kick=(4, 1), arms=(6, 10, 18), wave=(4, 1), point=25):
        """Built-wing fae only (she has limb bones): legs = (thigh forward, knee bend) with a slow alternate flutter
        kick = (degrees, cycles), toes pointed down `point`; arms = (forward, out, elbow) and a soft wave."""
        if "thigh_L" not in P:
            return
        for s, sfx in ((1, "L"), (-1, "R")):
            ph = 0.0 if s > 0 else math.pi
            k = math.sin(2 * math.pi * kick[1] * t + ph)
            P[f"thigh_{sfx}"].rotation_euler.x = R(legs[0] + kick[0] * k)
            P[f"shin_{sfx}"].rotation_euler.x = -R(legs[1] + 0.6 * kick[0] * (1 - k))  # more bent as the leg trails
            P[f"foot_{sfx}"].rotation_euler.x = -R(point)
            wv = math.sin(2 * math.pi * wave[1] * t + ph + 0.8)
            P[f"upperarm_{sfx}"].rotation_euler.x = R(arms[0] + wave[0] * wv)
            P[f"upperarm_{sfx}"].rotation_euler.z = s * R(arms[1])  # out from her sides
            P[f"forearm_{sfx}"].rotation_euler.x = R(arms[2] + 1.5 * wave[0] * wv)
            P[f"hand_{sfx}"].rotation_euler.x = R(0.5 * arms[2] + wave[0] * wv)

    def ease(e0, e1, t):
        x = min(1.0, max(0.0, (t - e0) / (e1 - e0)))
        return x * x * (3 - 2 * x)
    # floating, legs hanging soft with the toes pointed; flying, they trail behind her and flutter
    make_action("Idle", 2.0, lambda t: (fan(t, 2, 18, 0.02, sway=3), limbs(t)))
    make_action("Walk", 1.0, lambda t: (fan(t, 2, 26, 0.03, sway=4, pitch=6),
                                        limbs(t, legs=(-8, 18), kick=(5, 2), arms=(-8, 12, 20), wave=(5, 2))))
    make_action("Run", 0.8, lambda t: (fan(t, 3, 32, 0.03, sway=3, pitch=12),
                                       limbs(t, legs=(-14, 26), kick=(6, 3), arms=(-16, 16, 15), wave=(5, 3))))

    def attack(t):
        fan(t, 3, 35, 0.02)
        lunge = math.sin(math.pi * t)
        P["body"].rotation_euler.x = R(22) * lunge   # leans into the strike
        P["root"].location.x = 0.25 * lunge
        # both hands drawn back, then thrust at the target (she casts), then back to the float
        wind = ease(0.0, 0.35, t) * (1 - ease(0.35, 0.5, t))
        hit = ease(0.35, 0.5, t) * (1 - ease(0.6, 1.0, t))
        limbs(t, legs=(4 + 10 * hit, 12 + 20 * hit), kick=(2, 1), wave=(0, 1),
              arms=(6 - 30 * wind + 55 * hit, 10 + 15 * wind - 5 * hit, 18 + 60 * wind - 8 * hit))
    make_action("Attack", 0.9, attack)

    def flinch(t):
        fan(t, 1, 30, 0.0)
        j = math.sin(math.pi * t)
        P["body"].rotation_euler.x = -R(14) * j
        limbs(t, legs=(4 + 8 * j, 12 + 18 * j), kick=(0, 1), arms=(6 - 10 * j, 10 + 22 * j, 18 + 30 * j), wave=(0, 1))
    make_action("Hit", 0.35, flinch)

    def death(t):
        e = min(1.0, t / 0.8) ** 2
        P["root"].location.y = LIFT * (1 - e)
        P["body"].rotation_euler.x = -R(70) * e   # falls back
        P["wing_L"].rotation_euler.x = -R(30) * e
        P["wing_R"].rotation_euler.x = -R(30) * e  # mirrored bones: same sign droops both
        limbs(t, legs=(4 + 30 * e, 12 + 30 * e), kick=(0, 1), arms=(6 - 25 * e, 10 + 45 * e, 18 + 10 * e), wave=(0, 1),
              point=25 - 10 * e)  # arms flung out, legs left behind as she falls
    make_action("Death", 1.2, death)
else:
    HOVER = hover

    def flap(t, cycles, amp, bob, pitch=0.0):
        a = R(amp) * math.sin(2 * math.pi * cycles * t)
        # The wing bones mirror each other (local X runs +X on the left, -X on the right), so the same angle lifts both:
        # they beat up and down together, as any flier's do.
        P["wing_L"].rotation_euler.x = a
        P["wing_R"].rotation_euler.x = a
        P["root"].location.y = HOVER + bob * math.sin(2 * math.pi * cycles * t + 1.6)
        P["body"].rotation_euler.x = R(pitch)
    make_action("Idle", 1.0, lambda t: flap(t, 2, 30, 0.08))
    make_action("Walk", 1.0, lambda t: flap(t, 4, 45, 0.12, 3))
    make_action("Run", 0.6, lambda t: flap(t, 4, 55, 0.15, 6))

    def attack(t):
        flap(t, 6, 50, 0.1)
        P["root"].location.x = 0.25 * L * math.sin(math.pi * t)  # dart in and back
    make_action("Attack", 0.8, attack)
    make_action("Hit", 0.3, lambda t: (flap(t, 2, 60, 0.0), setattr(P["root"].location, "x", -0.1 * L * math.sin(math.pi * t))))

    def death(t):
        e = min(1.0, t / 0.7) ** 2
        P["root"].location.y = HOVER * (1 - e)
        P["root"].rotation_euler.y = R(50) * e
        P["wing_L"].rotation_euler.x = -R(25) * e
        P["wing_R"].rotation_euler.x = -R(25) * e
    make_action("Death", 1.0, death)

# ---- preview: mid-walk from the side and from the front three-quarter, framed to the model's size
if preview:
    rig.animation_data.action = bpy.data.actions["Walk" if kind == "worm" else "Idle"]
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
    d = 1.5 * max(L, W, 1.5)
    target = Vector((0, 0, 0.5 + (hover if kind == "moth" else 0)))
    for i, loc in enumerate([(0.0, -d, 0.6 * d * 0.3 + target.z), (0.75 * d, -0.55 * d, 0.45 * d + target.z)]):
        cam.location = loc
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = preview.replace(".png", f"_{i}.png")
        bpy.ops.render.render(write_still=True)

# Unreal's importer can't read textures embedded in the FBX: write them next to it as PNGs instead.
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
