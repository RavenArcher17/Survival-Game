# Blender (headless): render a model from four angles into one contact sheet.
#   blender -b -P render_preview.py -- <model.glb> <out.png>
import bpy, sys, math
from mathutils import Vector

model, out = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=model)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
# Frame everything.
lo = Vector((1e9, 1e9, 1e9)); hi = Vector((-1e9, -1e9, -1e9))
for o in meshes:
    for c in o.bound_box:
        w = o.matrix_world @ Vector(c)
        lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
center = (lo + hi) / 2; size = max(hi - lo)

scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE" if any(o.data.materials for o in meshes) else "MATERIAL"
scene.display.shading.show_cavity = True
scene.render.resolution_x = scene.render.resolution_y = 512
scene.render.film_transparent = False
world = bpy.data.worlds.new("w"); scene.world = world
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); scene.collection.objects.link(cam); scene.camera = cam
cam.data.lens = 50

shots = []
for i, (yaw, pitch) in enumerate([(30, 15), (90, 5), (210, 20), (0, 60)]):
    d = size * 2.2
    y, p = math.radians(yaw), math.radians(pitch)
    cam.location = center + Vector((math.cos(y) * math.cos(p), math.sin(y) * math.cos(p), math.sin(p))) * d
    cam.rotation_euler = (center - cam.location).to_track_quat("-Z", "Y").to_euler()
    path = out.replace(".png", f"_{i}.png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    shots.append(path)

# Stitch 2x2.
imgs = [bpy.data.images.load(p) for p in shots]
W = 512
sheet = bpy.data.images.new("sheet", W * 2, W * 2)
px = [0.0] * (W * 2 * W * 2 * 4)
for k, im in enumerate(imgs):
    src = list(im.pixels)
    ox, oy = (k % 2) * W, (1 - k // 2) * W
    for row in range(W):
        s = row * W * 4
        d = ((oy + row) * W * 2 + ox) * 4
        px[d:d + W * 4] = src[s:s + W * 4]
sheet.pixels = px
sheet.filepath_raw = out; sheet.file_format = "PNG"; sheet.save()
print("preview", out)
