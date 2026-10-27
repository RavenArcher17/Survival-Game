# Blender (headless): ready an image-to-3D prop for the game at a small size (polish pass base props).
#   blender -b -P normalize_prop.py -- <in.glb> <out.glb> [--faces 14000] [--tex 1024] [--yaw 0] [--crop 0.0]
# Joins the parts, cuts away a ground slab (--crop: fraction of the height), decimates to about --faces triangles,
# shrinks its texture to --tex pixels, turns it --yaw degrees about the vertical, and puts it centred over the origin
# with its bottom at zero. Keeps Git LFS small: a 40k-triangle prop with a 2k texture is ~5 MB, this ~1.5 MB.
import bpy, bmesh, sys, math
from mathutils import Vector, Matrix

argv = sys.argv[sys.argv.index("--") + 1:]
src, out = argv[0], argv[1]
opts = {"--faces": "14000", "--tex": "1024", "--yaw": "0", "--crop": "0"}
for i in range(2, len(argv) - 1, 2):
    opts[argv[i]] = argv[i + 1]
faces, tex, yaw, crop = int(opts["--faces"]), int(opts["--tex"]), float(opts["--yaw"]), float(opts["--crop"])

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
if len(meshes) > 1:
    bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

if crop > 0:
    zs = [v.co.z for v in obj.data.vertices]
    cut = min(zs) + (max(zs) - min(zs)) * crop
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < cut], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()

tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
if tris > faces:
    mod = obj.modifiers.new("decimate", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = faces / tris
    mod.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=mod.name)
after = sum(len(p.vertices) - 2 for p in obj.data.polygons)

for img in bpy.data.images:
    if img.size[0] > tex:
        img.scale(tex, tex)
        img.pack()

if yaw:
    obj.data.transform(Matrix.Rotation(math.radians(yaw), 4, "Z"))
lo = Vector((min(v.co.x for v in obj.data.vertices), min(v.co.y for v in obj.data.vertices), min(v.co.z for v in obj.data.vertices)))
hi = Vector((max(v.co.x for v in obj.data.vertices), max(v.co.y for v in obj.data.vertices), max(v.co.z for v in obj.data.vertices)))
obj.data.transform(Matrix.Translation(Vector((-(lo.x + hi.x) / 2, -(lo.y + hi.y) / 2, -lo.z))))
obj.data.update()
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=False)
size = hi - lo
print(f"normalized {src}: {tris} -> {after} triangles, size {size.x:.2f} x {size.y:.2f} x {size.z:.2f}, texture <= {tex}")
