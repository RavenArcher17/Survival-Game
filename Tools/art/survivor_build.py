# Blender (headless): build the game-ready player character from Mixamo output.
#   blender -b -P survivor_build.py -- <rigged_lowpoly.fbx> <fullres.fbx> <anim_dir> [<anim_dir> ...] --out <out.fbx> [--preview out.png]
# 1. The Mixamo rig was made on a decimated, untextured copy (Mixamo's page can't take our full file): copy its skin
#    weights onto the full-detail textured mesh (same shape, same place) and bind that mesh to the Mixamo skeleton.
# 2. Load every animation FBX (Mixamo "without skin" downloads for this character) onto the same skeleton, named after
#    its file (e.g. "SwordAndShieldSlash2"), with the hips' horizontal travel removed so they play in place.
# 3. Export mesh + skeleton + all animations as one FBX, textures as PNGs next to it (what Unreal's importer reads).
import bpy, sys, os, re, glob
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = args[args.index("--out") + 1]
preview = args[args.index("--preview") + 1] if "--preview" in args else None
rigged, fullres = args[0], args[1]
anim_dirs = [a for a in args[2:args.index("--out")]]

bpy.ops.wm.read_factory_settings(use_empty=True)


def imported(fn):
    before = set(bpy.data.objects)
    fn()
    return [o for o in bpy.data.objects if o not in before]


def action_curves(act):
    """F-curves of an action (Blender 5 keeps them in layers / strips / channelbags)."""
    if hasattr(act, "fcurves") and not hasattr(act, "layers"):
        return list(act.fcurves)
    return [fc for layer in act.layers for strip in layer.strips for bag in strip.channelbags for fc in bag.fcurves]


# ---- 1. skeleton from the rigged low-poly copy, weights onto the full mesh
new = imported(lambda: bpy.ops.import_scene.fbx(filepath=rigged, automatic_bone_orientation=False))
rig = next(o for o in new if o.type == "ARMATURE")
low = next(o for o in new if o.type == "MESH")
rig.name = "Armature"  # Unreal drops a root node with this name
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)

full = next(o for o in imported(lambda: bpy.ops.import_scene.fbx(filepath=fullres)) if o.type == "MESH")
full.name = "survivor"
bpy.context.view_layer.update()
# Put the full mesh in exactly the low mesh's space (Mixamo's file is in centimetres under the rotated, scaled armature),
# so the armature deforms it the same way.
full.data.transform(low.matrix_world.inverted() @ full.matrix_world)
for p in [p for p in bpy.data.objects if p.parent == full]:
    p.parent = None
full.parent = low.parent
full.matrix_parent_inverse = low.matrix_parent_inverse.copy()
full.location, full.rotation_euler, full.scale = low.location.copy(), low.rotation_euler.copy(), low.scale.copy()
bpy.context.view_layer.update()
for vg in low.vertex_groups:
    full.vertex_groups.new(name=vg.name)
dt = full.modifiers.new("weights", "DATA_TRANSFER")
dt.object = low
dt.use_vert_data = True
dt.data_types_verts = {"VGROUP_WEIGHTS"}
dt.vert_mapping = "POLYINTERP_NEAREST"
dt.layers_vgroup_select_src = "ALL"
dt.layers_vgroup_select_dst = "NAME"
bpy.context.view_layer.objects.active = full
bpy.ops.object.modifier_apply(modifier="weights")
bpy.ops.object.vertex_group_limit_total(limit=4)
bpy.ops.object.vertex_group_normalize_all(lock_active=False)
unweighted = sum(1 for v in full.data.vertices if not any(g.weight > 0 for g in v.groups))
arm_mod = full.modifiers.new("Armature", "ARMATURE")
arm_mod.object = rig
bpy.data.objects.remove(low)
print(f"weights: {len(full.vertex_groups)} groups on {len(full.data.vertices)} verts, unweighted {unweighted}, parent {full.parent.name if full.parent else None}")

# ---- rest pose: Mixamo's animation files are relative to its T-pose skeleton, but the rig was downloaded in the mesh's
# own pose (arms out and down). Pose the rig into the animations' rest skeleton, bake that into the mesh, and make it
# the new rest pose, so every animation plays as intended.
first_anim = next(p for d in anim_dirs for p in sorted(glob.glob(os.path.join(d, "*.fbx"))))
objs = imported(lambda: bpy.ops.import_scene.fbx(filepath=first_anim, automatic_bone_orientation=False))
ref = next(o for o in objs if o.type == "ARMATURE")
target = {b.name: b.matrix_local.copy() for b in ref.data.bones}
for o in objs:
    bpy.data.objects.remove(o)
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="POSE")
for pb in rig.pose.bones:  # parents come before children in this list
    if pb.name in target:
        pb.matrix = target[pb.name]
        bpy.context.view_layer.update()
bpy.ops.object.mode_set(mode="OBJECT")
bpy.context.view_layer.objects.active = full
bpy.ops.object.modifier_apply(modifier="Armature")  # mesh now stands in the T-pose
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode="POSE")
bpy.ops.pose.select_all(action="SELECT")
bpy.ops.pose.armature_apply(selected=False)
bpy.ops.object.mode_set(mode="OBJECT")
arm_mod = full.modifiers.new("Armature", "ARMATURE")
arm_mod.object = rig
worst = max(((rig.data.bones[n].matrix_local.to_translation() - m.to_translation()).length for n, m in target.items() if n in rig.data.bones), default=0)
print(f"rest pose now matches the animations' skeleton (worst bone offset {worst:.3f} units)")

# ---- 2. animations onto the same skeleton
rig.animation_data_create()
names = []
for d in anim_dirs:
    for path in sorted(glob.glob(os.path.join(d, "*.fbx"))):
        base = os.path.splitext(os.path.basename(path))[0]
        name = "".join(w.capitalize() for w in re.findall(r"[a-z0-9]+", base.lower()))  # "sword and shield slash (2)" -> SwordAndShieldSlash2
        objs = imported(lambda: bpy.ops.import_scene.fbx(filepath=path, automatic_bone_orientation=False))
        src = next((o for o in objs if o.type == "ARMATURE"), None)
        act = src.animation_data.action if src and src.animation_data else None
        if not act:
            print("no animation in", base)
        else:
            act.name = name
            act.use_fake_user = True
            # In place: freeze the hips' sideways (local X) and forward (local Z) travel at their first value;
            # keep local Y, the up-and-down (jumps, crouches, bob).
            for fc in action_curves(act):
                if fc.data_path == 'pose.bones["mixamorig:Hips"].location' and fc.array_index in (0, 2):
                    first = fc.keyframe_points[0].co[1]
                    for k in fc.keyframe_points:
                        k.co[1] = first
                        k.handle_left[1] = first
                        k.handle_right[1] = first
            names.append(name)
        for o in objs:
            bpy.data.objects.remove(o)
print("animations", len(names), names)

# ---- preview: the full mesh posed by an attack, to check the weights move it cleanly
if preview and names:
    pick = next((n for n in names if "Slash" in n), names[0])
    rig.animation_data.action = bpy.data.actions[pick]
    if hasattr(rig.animation_data, "action_suitable_slots") and rig.animation_data.action_suitable_slots:
        rig.animation_data.action_slot = rig.animation_data.action_suitable_slots[0]
    bpy.context.scene.frame_set(int(sum(bpy.data.actions[pick].frame_range) / 2))
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 700, 800
    scene.world = bpy.data.worlds.new("w")
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    for i, loc in enumerate([(0, -4.5, 1.2), (3.6, -2.8, 1.6)]):
        cam.location = loc
        cam.rotation_euler = (Vector((0, 0, 0.95)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = preview.replace(".png", f"_{i}.png")
        bpy.ops.render.render(write_still=True)
    print("preview pose", pick)
    rig.animation_data.action = None

# ---- 3. export (textures next to the FBX, as Unreal's importer can't read embedded ones)
for img in bpy.data.images:
    if img.packed_file or not img.filepath or not os.path.exists(bpy.path.abspath(img.filepath)):
        img.filepath_raw = os.path.join(os.path.dirname(os.path.abspath(out)), f"{os.path.splitext(os.path.basename(out))[0]}_{img.name}.png".replace(" ", "_"))
        img.file_format = "PNG"
        img.save()
        if img.packed_file:
            img.unpack(method="REMOVE")
bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"ARMATURE", "MESH"}, add_leaf_bones=False,
                         use_armature_deform_only=True, bake_anim=True, bake_anim_use_all_actions=True,
                         bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
                         mesh_smooth_type="FACE", path_mode="ABSOLUTE", embed_textures=False)
print("exported", out)
