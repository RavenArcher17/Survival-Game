# Blender (headless): rig image-to-3D creatures that aren't legged walkers, bake their animations, export FBX for Unreal.
#   blender -b -P rig_special.py -- <model.glb> <out.fbx> --body worm|moth|fae [--head +x|-x|+y|-y] [--hover 1.2] [--preview out.png]
#   worm  a 10-bone spine along the body's centreline: slithers with a travelling wave, rears up to strike.
#   moth  the concept is seen from above, so the model stands on its tail: it's laid flat (head forward, back up),
#         then gets a body, a head and one hinged wing per side; it hovers off the ground and flaps.
#   fae   an upright winged figure seen from the front (Ignessa): faced the camera, so it's turned to face +X; a body,
#         a head and a hinged wing per side behind the shoulders. It floats, sways and fans its wings (the game flies it).
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
if kind == "worm":
    # Joints follow the body's centreline, tail (-X) to head (+X).
    xs = np.linspace(lo[0] + 0.02 * L, hi[0] - 0.01 * L, SEGS + 1)
    joints = []
    for x in xs:
        sl = v[np.abs(v[:, 0] - x) < L / (2 * SEGS)]
        joints.append((x, float(np.median(sl[:, 1])), float((sl[:, 2].min() + sl[:, 2].max()) / 2)) if len(sl) else (x, 0.0, 0.5))
    prev = "root"
    for i in range(SEGS):
        bone(f"spine{i + 1}", joints[i], joints[i + 1], prev)
        prev = f"spine{i + 1}"
    print("centreline", [[round(c, 2) for c in j] for j in joints])
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
print("unweighted verts", unweighted, "of", len(body.data.vertices))

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
            if pb.name == "root":
                pb.keyframe_insert("location", frame=f + 1)
    act.use_fake_user = True

if kind == "worm":
    def wave(t, amp, k=0.7, lift=0.0):
        for i in range(SEGS):
            P[f"spine{i + 1}"].rotation_euler.z = R(amp) * math.sin(2 * math.pi * t - i * k)  # side to side
            P[f"spine{i + 1}"].rotation_euler.x = R(lift) * math.sin(2 * math.pi * t - i * k + 1.3)  # a little up and down
    make_action("Idle", 2.0, lambda t: wave(t, 4, lift=1.5))
    make_action("Walk", 1.0, lambda t: wave(t, 14, lift=2))
    make_action("Run", 0.6, lambda t: wave(t, 18, lift=3))

    def attack(t):
        rear = math.sin(math.pi * min(1.0, t / 0.45)) if t < 0.45 else 0.0   # rear the front up
        strike = math.sin(math.pi * (t - 0.4) / 0.35) if 0.4 < t < 0.75 else 0.0  # and slam forward
        for i in range(SEGS - 4, SEGS):
            P[f"spine{i + 1}"].rotation_euler.x = R(22) * rear - R(12) * strike
        P["root"].location.x = 0.08 * L * strike
    make_action("Attack", 0.9, attack)
    make_action("Hit", 0.3, lambda t: [setattr(P[f"spine{i + 1}"].rotation_euler, "z", R(8) * math.sin(math.pi * t)) for i in range(SEGS - 3, SEGS)])

    def death(t):
        e = min(1.0, t / 0.8) ** 2
        for i in range(SEGS):
            P[f"spine{i + 1}"].rotation_euler.z = R(14) * e  # curls up
        P["root"].location.y = -0.1 * e
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
    make_action("Idle", 2.0, lambda t: fan(t, 2, 18, 0.02, sway=3))
    make_action("Walk", 1.0, lambda t: fan(t, 2, 26, 0.03, sway=4, pitch=6))
    make_action("Run", 0.8, lambda t: fan(t, 3, 32, 0.03, sway=3, pitch=12))

    def attack(t):
        fan(t, 3, 35, 0.02)
        lunge = math.sin(math.pi * t)
        P["body"].rotation_euler.x = R(22) * lunge   # leans into the strike
        P["root"].location.x = 0.25 * lunge
    make_action("Attack", 0.9, attack)
    make_action("Hit", 0.35, lambda t: (fan(t, 1, 30, 0.0), setattr(P["body"].rotation_euler, "x", -R(14) * math.sin(math.pi * t))))

    def death(t):
        e = min(1.0, t / 0.8) ** 2
        P["root"].location.y = LIFT * (1 - e)
        P["body"].rotation_euler.x = -R(70) * e   # falls back
        P["wing_L"].rotation_euler.x = -R(30) * e
        P["wing_R"].rotation_euler.x = R(30) * e
    make_action("Death", 1.2, death)
else:
    HOVER = hover

    def flap(t, cycles, amp, bob, pitch=0.0):
        a = R(amp) * math.sin(2 * math.pi * cycles * t)
        P["wing_L"].rotation_euler.x = a
        P["wing_R"].rotation_euler.x = -a  # mirrored bone: same sign would flap one up, one down
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
        P["wing_R"].rotation_euler.x = R(25) * e
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
