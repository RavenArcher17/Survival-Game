# Blender (headless): which T-pose direction of a forearm ends up facing forward in given clips? Picks the shield's face
# direction for the game (player_art.json "shieldFace").
#   blender -b -P shield_axis.py -- <game.fbx> <bone> <ActionSubstring> [<ActionSubstring> ...]
import bpy, sys
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, bone = args[0], args[1]
wanted = args[2:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)
rig = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
pb = rig.pose.bones[bone]
rest = rig.matrix_world @ pb.bone.matrix_local
# Character facing: the hips' rest forward. Mixamo bodies built here face Blender's -Y.
fwd = Vector((0, -1, 0))
cands = {"up": Vector((0, 0, 1)), "forward": Vector((0, -1, 0)), "down": Vector((0, 0, -1)), "back": Vector((0, 1, 0)),
         "out": None}
side = (rig.matrix_world @ rig.pose.bones[bone.replace("ForeArm", "Hand")].bone.head_local) - (rig.matrix_world @ pb.bone.head_local)
cands["out"] = side.normalized()
for act in bpy.data.actions:
    if not any(w in act.name for w in wanted):
        continue
    rig.animation_data_create()
    rig.animation_data.action = act
    if hasattr(rig.animation_data, "action_suitable_slots") and rig.animation_data.action_suitable_slots:
        rig.animation_data.action_slot = rig.animation_data.action_suitable_slots[0]
    lo, hi = act.frame_range
    rows = []
    for t in (0.25, 0.5, 0.75):
        bpy.context.scene.frame_set(int(lo + (hi - lo) * t))
        bpy.context.view_layer.update()
        posed = rig.matrix_world @ pb.matrix
        delta = (posed.to_3x3() @ rest.to_3x3().inverted())
        rows.append({k: tuple(round(c, 2) for c in (delta @ v).normalized()) for k, v in cands.items() if k in ("up", "forward", "out")})
    print("AXIS", act.name.split("|")[-1], rows)
