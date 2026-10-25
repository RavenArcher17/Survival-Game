# Blender (headless): turn an image-to-3D humanoid (glb) into a Mixamo-ready FBX: one mesh, feet on the ground at the
# origin, scaled to a real height, facing front (-Y, Blender's front view), texture embedded in the FBX.
#   blender -b -P prep_mixamo.py -- <in.glb> <out.fbx> <height in metres> [--preview out.png]
import bpy, sys, math
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
src, out, height = args[0], args[1], float(args[2])
preview = args[args.index("--preview") + 1] if "--preview" in args else None

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
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
v = [p.co.copy() for p in body.data.vertices]
lo = Vector((min(c.x for c in v), min(c.y for c in v), min(c.z for c in v)))
hi = Vector((max(c.x for c in v), max(c.y for c in v), max(c.z for c in v)))
s = height / (hi.z - lo.z)
for p in body.data.vertices:
    p.co = Vector(((p.co.x - (lo.x + hi.x) / 2) * s, (p.co.y - (lo.y + hi.y) / 2) * s, (p.co.z - lo.z) * s))
body.data.update()
body.name = bpy.path.display_name_from_filepath(out)
print(f"prepared {body.name}: {len(body.data.polygons)} faces, {height} m tall, width {(hi.x - lo.x) * s:.2f} m")

if preview:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.color_type = "TEXTURE"
    scene.render.resolution_x, scene.render.resolution_y = 600, 800
    scene.world = bpy.data.worlds.new("w")
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    for i, ang in enumerate((0, 90)):  # front, side
        a = math.radians(ang)
        d = height * 2.2
        cam.location = (math.sin(a) * d, -math.cos(a) * d, height * 0.55)
        cam.rotation_euler = (Vector((0, 0, height * 0.5)) - cam.location).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = preview.replace(".png", f"_{i}.png")
        bpy.ops.render.render(write_still=True)

bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"MESH"}, path_mode="COPY", embed_textures=True,
                         mesh_smooth_type="FACE")
print("exported", out)
