# Blender (headless): hair grown over a rigged creature where a mask says (a mane, a ruff, a beard). Image models come
# out with hair as a smooth lump of the body; this covers the lump in locks: each one a tapered, flattened cone rooted
# just under the skin, lying along the skin with the flow, drooping toward its tip. A lock takes the colour under its
# root and its root's skin weights, so it moves with the skin.
# Run it on the rig (rig_quadruped.py's output), then make the clips from its output (anim_creature.py).
#   blender -b -P hair_locks.py -- <rigged.fbx> <mask.png> <out.fbx> [--count 1800] [--length 0.13] [--width 0.02]
#       [--flow -1,0,0] [--face x,y,z] [--radiate 1.0] [--lift 0.15] [--droop 0.7] [--jitter 0.07] [--seed 7]
#       [--along "x,z x,z ..."] [--material 0] [--mixamo] [--glb <preview.glb>]
# <mask.png>: on the model's UVs, white where hair grows. Sizes are shares of the creature's height. Locks are shorter
# toward the mask's edge (full length --length deep into it). The flow: --flow, plus (with --face, the face's middle,
# head +X) --radiate times the way out from the line through the face along the body: a lion's mane, combed out from
# the face and back, the crest up and back, the chest hair down. Roots on dark texels (shading baked into the texture's
# creases) are skipped, so no lock comes out dark. --along: a line in the side view (x,z points in the rig's frame, root
# first): each lock lies the way the line runs at its nearest point, in place of --flow and --face (a tail's fur combed
# toward its tip round the curve). --material: grow only on that material's faces (Ignessa's built wings are a
# second material whose UVs are the wing picture's, so the mask would land on them too). --glb also writes the result
# unrigged, for previews.
import bpy, bmesh, sys, math
import numpy as np
from mathutils import Vector
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
src, mask_path, out = args[0], args[1], args[2]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


def vec(name, default):
    return Vector([float(x) for x in opt(name, default).split(",")])


count = opt("--count", 1800)
length = opt("--length", 0.13)
width = opt("--width", 0.02)
flow = vec("--flow", "-1,0,0").normalized()
face = vec("--face", "0,0,0") if "--face" in args else None
radiate = opt("--radiate", 1.0)
lift = opt("--lift", 0.15)
droop = opt("--droop", 0.7)
jitter = opt("--jitter", 0.07)
along_line = None
if "--along" in args:
    pts = np.array([[float(c) for c in p.split(",")] for p in opt("--along", "").replace(";", " ").split()])
    seg = np.diff(pts, axis=0)
    along_line = (pts[:-1], seg, (seg ** 2).sum(1))
rng = np.random.default_rng(opt("--seed", 7))
SIDES, RINGS = 4, 4  # a diamond section (flat, a ridge down the middle), four rings and a point
TAPER = (0.85, 1.0, 0.75, 0.4)  # fuller just off the root, like a clump of hair

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
rig = next(o for o in bpy.data.objects if o.type == "ARMATURE")
obj = next(o for o in bpy.data.objects if o.type == "MESH")
mesh = obj.data
M = obj.matrix_world.copy()
Mi = M.inverted()
Mn = M.to_3x3().inverted().transposed()
mimg = bpy.data.images.load(mask_path)
MW, MH = mimg.size
marr = np.array(mimg.pixels[:], np.float32).reshape(MH, MW, mimg.channels)[..., 0]


def mask_at(uv):
    return float(marr[min(MH - 1, max(0, int(uv[1] * MH))), min(MW - 1, max(0, int(uv[0] * MW)))])


# the colour texture's brightness, and the same blurred (31 texels): a root much darker than round it is in a crease
cimg = next(n.image for m in mesh.materials if m and m.use_nodes for n in m.node_tree.nodes if n.type == "TEX_IMAGE" and n.image)
CW, CH = cimg.size
lum = np.array(cimg.pixels[:], np.float32).reshape(CH, CW, cimg.channels)[..., :3] @ np.array([0.3, 0.59, 0.11], np.float32)
blur = lum.copy()
for ax in (0, 1):
    c = np.cumsum(np.pad(blur, [(16, 15) if a == ax else (0, 0) for a in (0, 1)], mode="edge"), axis=ax)
    blur = (np.take(c, range(31, c.shape[ax]), axis=ax) - np.take(c, range(0, c.shape[ax] - 31), axis=ax)) / 31


def creased(uv):
    y, x = min(CH - 1, max(0, int(uv[1] * CH))), min(CW - 1, max(0, int(uv[0] * CW)))
    return lum[y, x] < 0.8 * blur[y, x]


uvs = mesh.uv_layers.active.data
co = [M @ v.co for v in mesh.vertices]
vn = [(Mn @ v.normal).normalized() for v in mesh.vertices]
H = max(c.z for c in co) - min(c.z for c in co)
vmask = np.zeros(len(co))
for lp in mesh.loops:
    vmask[lp.vertex_index] = max(vmask[lp.vertex_index], mask_at(uvs[lp.index].uv))
bare = KDTree(int((vmask < 0.3).sum()))
for i in np.nonzero(vmask < 0.3)[0]:
    bare.insert(co[i], int(i))
bare.balance()

mesh.calc_loop_triangles()
tris = list(mesh.loop_triangles)
weight = np.zeros(len(tris))
only_mat = opt("--material", -1)
for k, t in enumerate(tris):
    if only_mat >= 0 and mesh.polygons[t.polygon_index].material_index != only_mat:
        continue
    a, b, c = (co[i] for i in t.vertices)
    m = np.mean([mask_at(uvs[l].uv) for l in t.loops])
    weight[k] = 0.5 * (b - a).cross(c - a).length * (m if m > 0.3 else 0.0)
print(f"HAIR skin under the mask: {weight.sum() / H ** 2:.3f} (creature heights squared), {len(tris)} triangles")
groups_of = [dict((g.group, g.weight) for g in v.groups) for v in mesh.vertices]

bm = bmesh.new()
bm.from_mesh(mesh)
uvl = bm.loops.layers.uv.active
dl = bm.verts.layers.deform.verify()
down = Vector((0, 0, -1))
made = tries = 0
while made < count and tries < count * 6:
    tries += 1
    t = tris[rng.choice(len(tris), p=weight / weight.sum())]
    r1, r2 = rng.random(), rng.random()
    bary = (1 - math.sqrt(r1), math.sqrt(r1) * (1 - r2), math.sqrt(r1) * r2)
    uv = sum((Vector(uvs[l].uv) * w for l, w in zip(t.loops, bary)), Vector((0, 0)))
    if mask_at(uv) < 0.5 or creased(uv):
        continue
    p = sum((co[i] * w for i, w in zip(t.vertices, bary)), Vector())
    n = sum((vn[i] * w for i, w in zip(t.vertices, bary)), Vector()).normalized()
    # the way it lies: with the flow and out from the face, along the skin, lifted off it a little
    want = flow.copy()
    if along_line is not None:
        a0, sg, sl = along_line
        q = np.array([p.x, p.z])
        k = np.clip(((q - a0) * sg).sum(1) / sl, 0, 1)
        near = int(np.argmin((((a0 + sg * k[:, None]) - q) ** 2).sum(1)))
        want = Vector((sg[near][0], 0.0, sg[near][1])).normalized()
    elif face is not None:
        out_of_face = Vector((0.0, p.y - face.y, p.z - face.z))
        if out_of_face.length > 1e-6:
            r = out_of_face.normalized()
            want = flow + r * radiate * (1 - 0.6 * max(0.0, r.z))  # the crest combed back more than up
    along = want - n * want.dot(n)
    if along.length < 1e-4:
        along = n.orthogonal()
    d = (along.normalized() + n * lift + Vector(rng.normal(0, jitter, 3))).normalized()
    deep = bare.find(p)[2] / H
    L = length * H * (0.45 + 0.55 * min(1.0, deep / 0.05)) * rng.uniform(0.75, 1.25)
    w = width * H * rng.uniform(0.8, 1.25) * (0.75 + 0.25 * L / (length * H))
    twist = rng.uniform(-0.35, 0.35)
    # the spine: from just under the skin, each step drooping a little more
    pts, dirs = [p - n * 0.2 * w], [d]
    for k in range(RINGS):
        d = (d + down * droop / RINGS).normalized()
        pts.append(pts[-1] + d * L / RINGS)
        dirs.append(d)
    gw = {}
    for i, b in zip(t.vertices, bary):
        for g, x in groups_of[i].items():
            gw[g] = gw.get(g, 0.0) + x * b
    tot = sum(gw.values()) or 1.0
    rings = []
    for k in range(RINGS):
        side = dirs[k].cross(n)
        side = side.normalized() if side.length > 1e-6 else dirs[k].orthogonal().normalized()
        up = side.cross(dirs[k]).normalized()
        ring = []
        for j in range(SIDES):
            a = 2 * math.pi * j / SIDES + twist
            off = side * math.cos(a) * w * TAPER[k] + up * math.sin(a) * w * TAPER[k] * 0.4
            ring.append(bm.verts.new(Mi @ (pts[k] + off)))
        rings.append(ring)
    tip = bm.verts.new(Mi @ pts[-1])
    for v in [x for r in rings for x in r] + [tip]:
        for g, x in gw.items():
            v[dl][g] = x / tot
    faces = []  # (its corners, the spine point it wraps): faces face away from the spine
    for k in range(RINGS - 1):
        for j in range(SIDES):
            faces.append(([rings[k][j], rings[k][(j + 1) % SIDES], rings[k + 1][(j + 1) % SIDES], rings[k + 1][j]],
                          Mi @ ((pts[k] + pts[k + 1]) * 0.5)))
    for j in range(SIDES):
        faces.append(([rings[-1][j], rings[-1][(j + 1) % SIDES], tip], Mi @ pts[RINGS - 1]))
    for vs, axis in faces:
        f = bm.faces.new(vs)
        f.normal_update()
        if f.normal.dot(f.calc_center_median() - axis) < 0:
            f.normal_flip()
        f.material_index = t.material_index
        f.smooth = True
        for lp in f.loops:
            lp[uvl].uv = uv
    made += 1
bm.to_mesh(mesh)
bm.free()
print(f"HAIR {made} locks ({tries} tries), mesh now {len(mesh.vertices)} verts, {len(mesh.polygons)} faces")

for a in bpy.data.actions:
    a.name = a.name.split("|")[-1]  # the import names them <armature>|<take>; keep the rig's own take names
if "--mixamo" in args:  # a Mixamo-rigged boss: exported the way survivor_build.py exports (its axes, deform bones only)
    bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                             use_armature_deform_only=True, bake_anim=True, bake_anim_use_all_actions=True,
                             bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
                             mesh_smooth_type="FACE", path_mode="ABSOLUTE", embed_textures=False)
else:
    bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                             bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                             mesh_smooth_type="FACE", path_mode="ABSOLUTE", embed_textures=False, axis_forward="X", axis_up="Z")
print("HAIR exported", out, "actions", [a.name for a in bpy.data.actions])
if "--glb" in args:
    for m in list(obj.modifiers):
        obj.modifiers.remove(m)
    bpy.ops.export_scene.gltf(filepath=opt("--glb", ""), export_format="GLB", export_animations=False, export_skins=False,
                              use_selection=False)
    print("HAIR preview", opt("--glb", ""))
