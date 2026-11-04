# Blender (headless): decimate a humanoid FBX and pack its geometry compactly (zlib + base64) so it can be handed to a
# web page as text (Mixamo's page blocks fetching local files). Coordinates in mm, Y up, as int16; faces as uint16.
#   blender -b -P pack_for_page.py -- <in.fbx> <out.b64> [faces] [height in metres, default 1.8]
# Pack at the height prep_mixamo gave the full model: the rig comes back at the packed height, and survivor_build lays
# the full mesh over it as is, so a 2 m model packed at 1.8 m gets every joint 10% low (shoulders sunk into the chest).
import bpy, sys, struct, zlib, base64
import bmesh

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]
target = int(args[2]) if len(args) > 2 else 6000
height_mm = 1000.0 * (float(args[3]) if len(args) > 3 else 1.8)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
o = [x for x in bpy.context.scene.objects if x.type == "MESH"][0]
bpy.context.view_layer.objects.active = o
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
m = o.modifiers.new("dec", "DECIMATE")
m.ratio = min(1.0, target / max(1, len(o.data.polygons)))
bpy.ops.object.modifier_apply(modifier="dec")
bm = bmesh.new()
bm.from_mesh(o.data)
bmesh.ops.triangulate(bm, faces=bm.faces)
bm.verts.index_update()
zs = [v.co.z for v in bm.verts]
h = max(zs) - min(zs)
scale = height_mm / h if h > 0 else 1.0  # whatever the file's units, pack at the given height in mm
data = struct.pack("<II", len(bm.verts), len(bm.faces))
for v in bm.verts:
    data += struct.pack("<hhh", round(v.co.x * scale), round(v.co.z * scale), round(-v.co.y * scale))
for f in bm.faces:
    data += struct.pack("<HHH", *(v.index for v in f.verts))
packed = base64.b64encode(zlib.compress(data, 9)).decode()
open(out, "w").write(packed)
print(f"packed {len(bm.verts)} verts {len(bm.faces)} tris, height {h:.3f} units, {len(packed)} chars")
