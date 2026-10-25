# Blender (headless): import a game character FBX (mesh + skeleton + actions) and render a few actions mid-way from the
# front (-Y) and the side, to check the exported file itself.
#   blender -b -P check_actions.py -- <character.fbx> <out.png> <ActionSubstring> [<ActionSubstring> ...]
import bpy, sys
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out, wanted = args[0], args[1], args[2:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)
rig = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
mesh = next(o for o in bpy.context.scene.objects if o.type == "MESH")
print("CHK actions", len(bpy.data.actions), "e.g.", [a.name for a in bpy.data.actions][:4])
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.color_type = "TEXTURE"
scene.render.resolution_x, scene.render.resolution_y = 360, 480
scene.world = bpy.data.worlds.new("w")
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
scene.camera = cam
rig.animation_data_create()
shots = []
for w in wanted:
    act = next((a for a in bpy.data.actions if w.lower() in a.name.lower()), None)
    if not act:
        print("CHK missing", w)
        continue
    rig.animation_data.action = act
    if getattr(rig.animation_data, "action_slot", None) is None and getattr(rig.animation_data, "action_suitable_slots", None):
        rig.animation_data.action_slot = rig.animation_data.action_suitable_slots[0]
    scene.frame_set(int(act.frame_range[0] + (act.frame_range[1] - act.frame_range[0]) * 0.4))
    dg = bpy.context.evaluated_depsgraph_get()
    e = mesh.evaluated_get(dg)
    v = [e.matrix_world @ p.co for p in e.data.vertices]
    c = Vector([sum(p[i] for p in v) / len(v) for i in range(3)])
    print("CHK", act.name, "frames", tuple(act.frame_range), "centre", tuple(round(x, 2) for x in c), "height", round(max(p.z for p in v) - min(p.z for p in v), 2))
    for k, loc in enumerate([(0, -4.2, 1.1), (4.2, 0, 1.1)]):
        cam.location = Vector(loc) + Vector((c.x, c.y, 0))
        cam.rotation_euler = (Vector((c.x, c.y, 0.9)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = out.replace(".png", f"_{len(shots)}.png")
        bpy.ops.render.render(write_still=True)
        shots.append(scene.render.filepath)
try:
    from PIL import Image
except ImportError:
    Image = None
print("CHK shots", len(shots))
