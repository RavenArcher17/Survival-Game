# Blender (headless): cut away the bottom of a prop (image-to-3D sometimes adds a plinth or ground slab) and re-export.
#   blender -b -P crop_bottom.py -- <in.glb> <out.glb> <fraction of height to remove, e.g. 0.07>
import bpy, sys
import bmesh

src, out, frac = sys.argv[sys.argv.index("--") + 1:][:3]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
for o in [o for o in bpy.context.scene.objects if o.type == "MESH"]:
    zs = [(o.matrix_world @ v.co).z for v in o.data.vertices]
    cut = min(zs) + (max(zs) - min(zs)) * float(frac)
    bm = bmesh.new()
    bm.from_mesh(o.data)
    doomed = [v for v in bm.verts if (o.matrix_world @ v.co).z < cut]
    bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(o.data)
    bm.free()
    print("cropped", o.name, "below", round(cut, 3), "removed", len(doomed), "verts")
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB")
