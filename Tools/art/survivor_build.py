# Blender (headless): build the game-ready player character from Mixamo output.
#   blender -b -P survivor_build.py -- <rigged_lowpoly.fbx> <fullres.fbx> <anim_dir> [<anim_dir> ...] --out <out.fbx> [--preview out.png] [--cut-stretch 2.5]
# 1. The Mixamo rig was made on a decimated, untextured copy (Mixamo's page can't take our full file): copy its skin
#    weights onto the full-detail textured mesh (same shape, same place) and bind that mesh to the Mixamo skeleton.
# 2. Load every animation FBX (Mixamo "without skin" downloads for this character) onto the same skeleton, named after
#    its file (e.g. "SwordAndShieldSlash2"), with the hips' horizontal travel removed so they play in place (and the
#    vertical travel too for jumps).
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
if "--smooth-weights" in args:
    # Spiky or furry image-to-3D bodies pick up blotchy weights from the decimated copy; blur them over the surface
    # so spikes and cuffs bend with their neighbours instead of tearing.
    bpy.ops.object.mode_set(mode="WEIGHT_PAINT")
    bpy.ops.object.vertex_group_smooth(group_select_mode="ALL", factor=0.5, repeat=int(args[args.index("--smooth-weights") + 1]))
    bpy.ops.object.mode_set(mode="OBJECT")
bpy.ops.object.vertex_group_limit_total(limit=4)
bpy.ops.object.vertex_group_normalize_all(lock_active=False)
unweighted = sum(1 for v in full.data.vertices if not any(g.weight > 0 for g in v.groups))
arm_mod = full.modifiers.new("Armature", "ARMATURE")
arm_mod.object = rig
bpy.data.objects.remove(low)
print(f"weights: {len(full.vertex_groups)} groups on {len(full.data.vertices)} verts, unweighted {unweighted}, parent {full.parent.name if full.parent else None}")


def edge_lengths(co):
    """Length of every mesh edge for vertex positions co (n x 3)."""
    import numpy as np
    e = np.empty(len(full.data.edges) * 2, dtype=np.int64)
    full.data.edges.foreach_get("vertices", e)
    e = e.reshape(-1, 2)
    return np.linalg.norm(co[e[:, 0]] - co[e[:, 1]], axis=1)


def mesh_co(mesh):
    import numpy as np
    co = np.empty(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


# Edge lengths in the model's own pose: the yardstick for --cut-stretch (the move to the T-pose below already stretches
# any fused hand-to-thigh bridge).
orig_len = edge_lengths(mesh_co(full.data)) if "--cut-stretch" in args else None

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
            # Jumps also lose their vertical travel: the game lifts the capsule itself, so the clip's own hop doubled it.
            frozen = (0, 1, 2) if "Jump" in name else (0, 2)
            for fc in action_curves(act):
                if fc.data_path == 'pose.bones["mixamorig:Hips"].location' and fc.array_index in frozen:
                    first = fc.keyframe_points[0].co[1]
                    for k in fc.keyframe_points:
                        k.co[1] = first
                        k.handle_left[1] = first
                        k.handle_right[1] = first
            names.append(name)
        for o in objs:
            bpy.data.objects.remove(o)
print("animations", len(names), names)

# ---- optional: cut faces that tear when animated. Image-to-3D bodies sometimes fuse a hand to a thigh (hands hanging
# at the sides); skinned, those bridges stretch into long planks whenever the arm moves. Play every clip, measure each
# edge against its rest length, and delete the faces on any edge that ever stretches past the given ratio.
if "--cut-stretch" in args:
    import numpy as np
    import bmesh
    limit = float(args[args.index("--cut-stretch") + 1])
    deps = bpy.context.evaluated_depsgraph_get()
    edges = np.empty(len(full.data.edges) * 2, dtype=np.int64)
    full.data.edges.foreach_get("vertices", edges)
    edges = edges.reshape(-1, 2)
    rest_len = np.maximum(orig_len, 1e-6)
    # Only what the clips do counts: the T-pose bind itself tears shoulders it never shows in play.
    worst = np.ones(len(edges))
    for name in names:
        act = bpy.data.actions[name]
        rig.animation_data.action = act
        if hasattr(rig.animation_data, "action_suitable_slots") and rig.animation_data.action_suitable_slots:
            rig.animation_data.action_slot = rig.animation_data.action_suitable_slots[0]
        lo, hi = act.frame_range
        for f in np.linspace(lo, hi, 8):
            bpy.context.scene.frame_set(int(round(f)))
            ev = full.evaluated_get(deps)
            co = mesh_co(ev.to_mesh())
            ev.to_mesh_clear()
            worst = np.maximum(worst, edge_lengths(co) / rest_len)
    rig.animation_data.action = None
    bpy.context.scene.frame_set(0)
    bad = {tuple(sorted(e)) for e in edges[worst > limit].tolist()}
    bm = bmesh.new()
    bm.from_mesh(full.data)
    doomed = [f for f in bm.faces if any(tuple(sorted((e.verts[0].index, e.verts[1].index))) in bad for e in f.edges)]
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    # The cut leaves scraps of the bridges floating: drop pieces under half a percent of the faces.
    bm.faces.ensure_lookup_table()
    seen, scraps = set(), []
    for start in bm.faces:
        if start.index in seen:
            continue
        island, stack = [], [start]
        seen.add(start.index)
        while stack:
            f = stack.pop()
            island.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        if len(island) < 0.005 * len(bm.faces):
            scraps += island
    bmesh.ops.delete(bm, geom=scraps, context="FACES")
    doomed += scraps
    # Close the holes the cut leaves, each loop on its own (a cut hand-to-thigh bridge closes the hand and the thigh
    # separately). The patches take texture coordinates from the faces around each corner.
    made = bmesh.ops.holes_fill(bm, edges=[e for e in bm.edges if e.is_boundary], sides=0)["faces"]
    made_set = set(made)
    uv = bm.loops.layers.uv.active
    if uv:
        for f in made:
            for loop in f.loops:
                around = [l[uv].uv.copy() for l in loop.vert.link_loops if l.face not in made_set]
                if around:
                    loop[uv].uv = sum(around, Vector((0.0, 0.0))) / len(around)
    patches = len(bmesh.ops.triangulate(bm, faces=made)["faces"]) if made else 0
    print(f"cut-stretch: {len(made)} holes closed ({patches} patch faces)")
    bm.to_mesh(full.data)
    bm.free()
    full.data.update()
    print(f"cut-stretch: {len(bad)} edges stretched past {limit}x, {len(doomed)} faces removed of {len(full.data.polygons) + len(doomed)}")

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
for img in [i for i in bpy.data.images if i.type == "IMAGE"]:  # not the preview's Render Result
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
