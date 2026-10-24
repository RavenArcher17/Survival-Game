# Blender (headless): rig an image-to-3D quadruped and bake its basic animations, then export FBX for Unreal.
#   blender -b -P rig_quadruped.py -- <model.glb> <out.fbx> [--head +x|-x|+y|-y] [--gait trot|hop] [--stretch 1.3] [--preview out.png]
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
v = np.array([p.co[:] for p in body.data.vertices])
lo, hi = v.min(0), v.max(0)
L = hi[0] - lo[0]

# ---- legs: k-means on the low vertices, seeded by the four quadrants
def find_feet(cut):
    """Rasterize the vertices below `cut` onto a 1.5 cm grid and return the centres of the 4 biggest islands."""
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
    return [low[m].mean(0) for m in islands[:4]], [len(m) for m in islands[:6]]

def feet_now():
    for cut in (0.06, 0.1, 0.15, 0.2):  # lowest slice that shows four separate feet
        feet, sizes = find_feet(cut)
        if len(feet) == 4 and sizes[3] > 20:
            return np.array(feet), cut, sizes
    return np.array(feet), cut, sizes

# Image-to-3D creatures often stand at an angle (three-quarter concept art): turn the body so the line from the
# rear feet to the front feet runs along +X, then find the feet again.
centers, cut, sizes = feet_now()
by_x = centers[np.argsort(-centers[:, 0])]
fwd = by_x[:2].mean(0) - by_x[2:].mean(0)
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
# Three-quarter concepts come out foreshortened (short bodies): --stretch lengthens only the torso between the inner
# edges of the rear and front feet, so legs, head and tail keep their shape.
if stretch != 1.0:
    xs = np.sort(centers[:, 0])
    rx0, fx0 = xs[1] + 0.05, xs[2] - 0.05
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
lab = np.zeros(1, int)
print("feet slice", cut, "island sizes", sizes)
names = ["FL", "FR", "RL", "RR"]  # front/rear, left (+y) / right
order = sorted(range(4), key=lambda k: -centers[k][0])
front, rear = sorted(order[:2], key=lambda k: -centers[k][1]), sorted(order[2:], key=lambda k: -centers[k][1])
legs = dict(zip(names, [centers[front[0]], centers[front[1]], centers[rear[0]], centers[rear[1]]]))
print("legs", {k: [round(float(c[0]), 3), round(float(c[1]), 3)] for k, c in legs.items()})

# ---- measure the body instead of assuming proportions (tortoises, giraffes and cats all differ)
fx = (legs["FL"][0] + legs["FR"][0]) / 2
rx = (legs["RL"][0] + legs["RR"][0]) / 2
mid = v[(v[:, 0] > rx + 0.15 * (fx - rx)) & (v[:, 0] < fx - 0.15 * (fx - rx)) & (np.abs(v[:, 1]) < 0.12)]
belly = float(np.percentile(mid[:, 2], 3)) if len(mid) else 0.35
back = float(mid[:, 2].max()) if len(mid) else 0.8
hip_z = float(np.clip(belly + 0.45 * (back - belly), 0.2, 0.8))
knee_z = max(belly * 0.55, hip_z * 0.4)  # squat critters still get a real knee
ankle_z = max(0.04, knee_z * 0.3)
cap_f = v[v[:, 0] > hi[0] - 0.12 * L]
cap_r = v[v[:, 0] < lo[0] + 0.10 * L]
head_tip = (float(hi[0]) - 0.01, 0.0, float(cap_f[:, 2].mean()))
tail_tip = (float(lo[0]) + 0.01, 0.0, float(cap_r[:, 2].mean()))
print(f"body belly {belly:.2f} back {back:.2f} hip {hip_z:.2f} head z {head_tip[2]:.2f} tail z {tail_tip[2]:.2f}")

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

front_x = (legs["FL"][0] + legs["FR"][0]) / 2
rear_x = (legs["RL"][0] + legs["RR"][0]) / 2
bone("root", (0, 0, 0), (0, 0, 0.2))
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
for n, (x, y) in legs.items():
    par = "spine3" if n[0] == "F" else "pelvis"
    side = (0, 1, 0)  # leg bones swing about their local Z (= world lateral axis)
    bone(f"{n}_upper", (x, y, hip_z - 0.02), (x - 0.02, y, knee_z), par, side)
    bone(f"{n}_lower", (x - 0.02, y, knee_z), (x, y, ankle_z), f"{n}_upper", side)
    bone(f"{n}_foot", (x, y, ankle_z), (x + 0.06, y, 0.0), f"{n}_lower", side)
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
        b.envelope_distance = 0.12
        b.head_radius = b.tail_radius = 0.06
    bpy.ops.object.parent_set(type="ARMATURE_ENVELOPE")
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

def gait(t, swing, knee, bob):
    for n, ph in phase.items():
        a = 2 * math.pi * (t + ph)
        P[f"{n}_upper"].rotation_euler.z = R(swing) * math.sin(a)
        P[f"{n}_lower"].rotation_euler.z = -R(knee) * max(0.0, math.sin(a + 1.2))
        P[f"{n}_foot"].rotation_euler.z = R(knee * 0.5) * max(0.0, math.sin(a + 1.6))
    P["root"].location.z = bob * abs(math.sin(2 * math.pi * t * 2))
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
    P["root"].location.z = -0.05 * e
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
