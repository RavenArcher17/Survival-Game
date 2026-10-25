# Blender (headless): make an image-to-3D held item ready to attach to a hand bone.
#   blender -b -P normalize_gear.py -- sword <in.glb> <out.glb> <length cm>
#   blender -b -P normalize_gear.py -- shield <in.glb> <out.glb> <diameter cm>
#   blender -b -P normalize_gear.py -- pole <in.glb> <out.glb> <length cm> [--front wide|narrow] [--grip 0.4]
#   blender -b -P normalize_gear.py -- bow <in.glb> <out.glb> <length cm>    /    -- ball <in.glb> <out.glb> <size cm>
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


opt = sys.argv[sys.argv.index("--") + 1:][4:]


def arg(name, default):
    return opt[opt.index(name) + 1] if name in opt else default


if kind == "pole":
    # A long item (crossbow, blowgun, spear, arrow): its front along +Y, the next widest axis along X, and the grip
    # (a fraction of the way from the back end) at the origin. --front wide|narrow: which end is the front (a spear's
    # head and a crossbow's prod are the wide end; an arrow's fletching is its wide end, so its front is narrow).
    # Image-to-3D often leaves the item lying on a slant (the concept was drawn diagonally), so it is first turned onto
    # its principal axes: longest along Y, next along X, thinnest along Z.
    c = v.mean(axis=0)
    _, _, vt = np.linalg.svd(v - c, full_matrices=False)
    obj.data.transform(basis(vt[1], vt[0], np.cross(vt[1], vt[0])) @ Matrix.Translation(-Vector(c)))
    v = np.array([vert.co[:] for vert in obj.data.vertices])
    long_ax = 1
    others = [a for a in range(3) if a != long_ax]
    t = v[:, long_ax]
    t0, t1 = t.min(), t.max()

    def end_width(a, b):
        sl = v[(t >= a) & (t <= b)][:, others]
        return (sl.max(axis=0) - sl.min(axis=0)).max() if len(sl) else 0.0

    span = t1 - t0
    low_w, high_w = end_width(t0, t0 + 0.15 * span), end_width(t1 - 0.15 * span, t1)
    wide_high = high_w >= low_w
    front_high = wide_high if arg("--front", "wide") == "wide" else not wide_high
    sign = 1.0 if front_high else -1.0
    back = t0 if front_high else t1
    grip = back + sign * span * float(arg("--grip", "0.4"))
    widths = v[:, others].max(axis=0) - v[:, others].min(axis=0)
    side_ax = others[int(np.argmax(widths))]
    centre = (v.min(axis=0) + v.max(axis=0)) / 2
    centre[long_ax] = grip
    obj.data.transform(Matrix.Translation(-Vector(centre)))
    ax = np.eye(3)
    y = ax[long_ax] * sign
    x = ax[side_ax]
    obj.data.transform(basis(x, y, np.cross(x, y)))
    obj.data.transform(Matrix.Scale(size / span, 4))
    print(f"GEAR pole: long axis {long_ax}, ends {low_w:.3f}/{high_w:.3f} wide, front {'high' if front_high else 'low'}, grip {grip:.3f}, length {span:.3f} -> {size} m")
elif kind == "bow":
    # Limbs along +Y, the arrow's flight (string to riser) along +X, the riser's grip at the origin.
    long_ax = int(np.argmax(ext))
    others = sorted([a for a in range(3) if a != long_ax], key=lambda a: -ext[a])
    arrow_ax, thin_ax = others
    t = v[:, long_ax]
    mid = (t.min() + t.max()) / 2
    sl = v[np.abs(t - mid) < 0.08 * (t.max() - t.min())][:, arrow_ax]
    med = float(np.median(sl))
    string_side = sl.max() if abs(sl.max() - med) > abs(sl.min() - med) else sl.min()
    flight = 1.0 if med > string_side else -1.0  # from the string toward the riser
    centre = (v.min(axis=0) + v.max(axis=0)) / 2
    centre[long_ax] = mid
    centre[arrow_ax] = med
    obj.data.transform(Matrix.Translation(-Vector(centre)))
    ax = np.eye(3)
    x = ax[arrow_ax] * flight
    y = ax[long_ax]
    obj.data.transform(basis(x, y, np.cross(x, y)))
    obj.data.transform(Matrix.Scale(size / (t.max() - t.min()), 4))
    print(f"GEAR bow: limbs axis {long_ax}, arrow axis {arrow_ax} (flight {flight:+.0f}), riser at {med:.3f}")
elif kind == "ball":
    # Something held by its middle (a bola): centred, scaled to its widest extent.
    centre = (v.min(axis=0) + v.max(axis=0)) / 2
    obj.data.transform(Matrix.Translation(-Vector(centre)))
    obj.data.transform(Matrix.Scale(size / float(ext.max()), 4))
    print(f"GEAR ball: {float(ext.max()):.3f} across -> {size} m")
elif kind == "sword":
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
