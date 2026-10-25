# Blender (headless): make an image-to-3D held item ready to attach to a hand bone.
#   blender -b -P normalize_gear.py -- sword <in.glb> <out.glb> <length cm>
#   blender -b -P normalize_gear.py -- shield <in.glb> <out.glb> <diameter cm>
# sword:  blade along +Y, the middle of the grip (between the crossguard and the pommel end) at the origin, crossguard
#         width along X.
# shield: face along +Z (front, the side with the boss), centre of the disc at the origin. Anything sticking out of the
#         back more than a few centimetres (the generator likes to add stray handle struts) is cut off.
# Units are metres in Blender; the glTF importer in Unreal turns them into centimetres.
import bpy, bmesh, sys
import numpy as np
from mathutils import Matrix, Vector

kind, src, out, size_cm = sys.argv[sys.argv.index("--") + 1:][:4]
size = float(size_cm) / 100.0
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
obj.parent = None
obj.data.transform(obj.matrix_world)
obj.matrix_world = Matrix.Identity(4)
for o in [o for o in bpy.context.scene.objects if o.type != "MESH"]:
    bpy.data.objects.remove(o)

v = np.array([vert.co[:] for vert in obj.data.vertices])
lo, hi = np.percentile(v, 2, axis=0), np.percentile(v, 98, axis=0)
ext = hi - lo


def basis(x, y, z):
    """Rotation taking the given (orthonormal) source axes to world X, Y, Z."""
    return Matrix((Vector(x), Vector(y), Vector(z))).to_4x4()


if kind == "sword":
    long_ax = int(np.argmax(ext))
    others = [a for a in range(3) if a != long_ax]
    t = v[:, long_ax]
    edges = np.linspace(t.min(), t.max(), 61)
    widths = []
    for a, b in zip(edges[:-1], edges[1:]):
        sl = v[(t >= a) & (t < b)][:, others]
        widths.append((sl.max(axis=0) - sl.min(axis=0)).max() if len(sl) else 0.0)
    g = int(np.argmax(widths))  # the crossguard is the widest slice
    guard = (edges[g] + edges[g + 1]) / 2
    sl = v[(t >= edges[g]) & (t < edges[g + 1])][:, others]
    guard_ax = others[int(np.argmax(sl.max(axis=0) - sl.min(axis=0)))]
    flat_ax = [a for a in others if a != guard_ax][0]
    pommel_end = t.min() if abs(guard - t.min()) < abs(t.max() - guard) else t.max()
    blade_sign = 1.0 if pommel_end == t.min() else -1.0
    grip = (guard + pommel_end) / 2
    centre = np.zeros(3)
    centre[long_ax] = grip
    centre[others[0]] = (v[:, others[0]].min() + v[:, others[0]].max()) / 2
    centre[others[1]] = (v[:, others[1]].min() + v[:, others[1]].max()) / 2
    obj.data.transform(Matrix.Translation(-Vector(centre)))
    ax = np.eye(3)
    y = ax[long_ax] * blade_sign
    x = ax[guard_ax]
    z = np.cross(x, y)
    obj.data.transform(basis(x, y, z))
    length = t.max() - t.min()
    obj.data.transform(Matrix.Scale(size / length, 4))
    print(f"GEAR sword: long axis {long_ax}, guard at {guard:.3f}, pommel end {pommel_end:.3f}, grip {grip:.3f}, length {length:.3f} -> {size} m")
else:
    thin_ax = int(np.argmin(ext))
    t = v[:, thin_ax]
    mid = np.median(t)
    half = (np.percentile(t, 90) - np.percentile(t, 10)) / 2
    back_sign = 1.0 if (t.max() - mid) > (mid - t.min()) else -1.0  # the struts stick out further than the boss
    cut = mid + back_sign * (half + 0.04 * ext.max())
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    no = [0.0, 0.0, 0.0]
    no[thin_ax] = back_sign
    co = [0.0, 0.0, 0.0]
    co[thin_ax] = cut
    res = bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=co, plane_no=no, clear_outer=True)
    bm.to_mesh(obj.data)
    bm.free()
    v = np.array([vert.co[:] for vert in obj.data.vertices])
    others = [a for a in range(3) if a != thin_ax]
    centre = (v.min(axis=0) + v.max(axis=0)) / 2
    diameter = max(v[:, others[0]].max() - v[:, others[0]].min(), v[:, others[1]].max() - v[:, others[1]].min())
    obj.data.transform(Matrix.Translation(-Vector(centre)))
    ax = np.eye(3)
    z = -ax[thin_ax] * back_sign  # front = away from the back
    up_ax = 2 if thin_ax != 2 else 1
    y = ax[up_ax]
    x = np.cross(y, z)
    obj.data.transform(basis(x, y, z))
    obj.data.transform(Matrix.Scale(size / diameter, 4))
    print(f"GEAR shield: thin axis {thin_ax}, back side {back_sign:+.0f}, cut at {cut:.3f} (median {mid:.3f}), diameter {diameter:.3f} -> {size} m")

obj.data.update()
bpy.ops.export_scene.gltf(filepath=out, export_format="GLB", use_selection=False)
print("GEAR exported", out)
