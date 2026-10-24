# Blender (headless): sculpt a natural cave - a rocky mound with a domed chamber and a rough entrance tunnel carved out -
# and export it as a static mesh FBX for Unreal (AWRCave uses it instead of block walls).
#   blender -b -P cave_gen.py -- <out.fbx> [--seed 1] [--preview out.png]
# Local space matches AWRCave: chamber centred on the origin (floor ~z 0, dome ~10.5 m high, ~12 m radius), the tunnel
# runs out along +X. There is no modelled floor: the walls go 3 m underground and the landscape is the floor, so uneven
# ground inside never pokes through. Units are metres (Unreal reads them as 100 cm).
import bpy, sys, math, random
import bmesh
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
seed = int(args[args.index("--seed") + 1]) if "--seed" in args else 1
preview = args[args.index("--preview") + 1] if "--preview" in args else None
rnd = random.Random(seed)
CUT = -3.0  # walls end this far underground

bpy.ops.wm.read_factory_settings(use_empty=True)


def apply_all(obj):
    bpy.context.view_layer.objects.active = obj
    for m in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)


def rough(obj, small, large, coarse_size=7.0, fine_size=1.4):
    """Noise along the normals, in world metres: big lumps, angular rock facets (Voronoi cell edges), small knobs."""
    layers = [("coarse", "CLOUDS", coarse_size, large), ("facets", "VORONOI", coarse_size * 0.45, large * 0.55),
              ("fine", "CLOUDS", fine_size, small)]
    for name, kind, size, strength in layers:
        tex = bpy.data.textures.new(f"{obj.name}_{name}", type=kind)
        tex.noise_scale = size
        if kind == "VORONOI":
            tex.weight_1, tex.weight_2 = -1.0, 1.0  # F2 - F1: flat faces meeting at ridges, like broken rock
            tex.noise_intensity = 2.0
        else:
            tex.noise_depth = 3
        m = obj.modifiers.new(name, "DISPLACE")
        m.texture = tex
        m.texture_coords = "GLOBAL"
        m.strength = strength
        m.mid_level = 0.5
    apply_all(obj)


def blob(name, centre, radii, subdiv, small, large):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdiv, radius=1.0, location=centre)
    o = bpy.context.active_object
    o.name = name
    o.scale = radii
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    # Shift the noise per object and seed so caves differ.
    rough(o, small, large)
    return o


def tube(name, x0, x1, r_in, r_out, squash, z):
    """Entrance tunnel along +X, flaring from r_in (chamber end) to r_out (mouth), dense enough for the noise."""
    bpy.ops.mesh.primitive_cone_add(vertices=48, radius1=r_in, radius2=r_out, depth=x1 - x0, location=((x0 + x1) / 2, 0, z), rotation=(0, math.pi / 2, 0))
    o = bpy.context.active_object
    o.name = name
    bpy.context.view_layer.objects.active = o
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.subdivide(number_cuts=24)
    bpy.ops.object.mode_set(mode="OBJECT")
    o.scale = (1, 1, squash)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    rough(o, 0.35, 0.9, coarse_size=4.0, fine_size=1.2)
    return o


def boolean(target, other, op):
    """Exact boolean; if a rough input defeats it (the result comes back empty), redo it with the fast solver."""
    before = target.data.copy()
    for solver in ("EXACT", "FAST"):
        m = target.modifiers.new(op, "BOOLEAN")
        m.operation = op
        m.object = other
        m.solver = solver
        apply_all(target)
        if len(target.data.polygons) > 0:
            break
        target.data = before.copy()
    print(f"  boolean {op} {other.name} ({solver}): {len(before.polygons)} -> {len(target.data.polygons)} faces")
    bpy.data.objects.remove(other)


# Each cave's noise sits somewhere else in the noise field.
offset = Vector((rnd.uniform(-500, 500), rnd.uniform(-500, 500), 0))

# ---- the outer mound: a main dome plus a few lumps so it reads as a rock outcrop, not a bowl
mound = blob("Cave", offset + Vector((0, 0, -1.5)), (17.5, 17.0, 16.5), 6, 0.45, 3.2)
# Lumps of varied size around the sides and back (never in front of the entrance, +X).
lumps = []
while len(lumps) < 5:
    a = rnd.uniform(0.35, 2 * math.pi - 0.35)  # angle from +X, skipping the entrance side
    d = rnd.uniform(9, 15)
    lumps.append((Vector((math.cos(a) * d, math.sin(a) * d, rnd.uniform(-3, 1))), rnd.uniform(5.5, 11)))
for i, (c, r) in enumerate(lumps):
    boolean(mound, blob(f"Lump{i}", offset + c, (r, r * rnd.uniform(0.75, 1.2), r * rnd.uniform(0.7, 1.1)), 5, 0.4, 2.2), "UNION")
mound.location -= offset  # back to the origin (noise was sampled at the offset)
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)

# ---- carve the chamber (dome ~10.5 m high, ~12 m radius at the floor) and the tunnel out to +X
chamber = blob("Chamber", offset + Vector((0, 0, -3)), (12.2, 12.2, 13.5), 5, 0.35, 0.8)
chamber.location -= offset
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
boolean(mound, chamber, "DIFFERENCE")
# The tunnel is a long rough ellipsoid from inside the chamber out past the front face (~6 m wide, top ~4.5 m).
tun = blob("Tunnel", offset + Vector((15.5, 0, 1.2)), (9.5, 3.2, 3.6), 5, 0.3, 0.7)
tun.location -= offset
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
boolean(mound, tun, "DIFFERENCE")
# A hollow scooped out of the front face, so the entrance is a ragged recess in the rock rather than a drilled hole.
porch = blob("Porch", offset + Vector((17.5, 0, 1.5)), (6.0, 7.5, 6.5), 5, 0.35, 1.4)
porch.location -= offset
bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
boolean(mound, porch, "DIFFERENCE")

# ---- cut everything below CUT (open bottom: the landscape is the floor)
bm = bmesh.new()
bm.from_mesh(mound.data)
bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, CUT), plane_no=(0, 0, 1), clear_inner=True)
bm.to_mesh(mound.data)
bm.free()

# ---- tidy: fewer triangles, smooth-ish shading, a UV set, one material slot
dec = mound.modifiers.new("dec", "DECIMATE")
dec.ratio = min(1.0, 90000 / max(1, len(mound.data.polygons)))
apply_all(mound)
bpy.ops.object.shade_smooth()
bpy.context.view_layer.objects.active = mound
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.normals_make_consistent(inside=False)
bpy.ops.object.mode_set(mode="OBJECT")
# Box-projected UVs, 4 m tiles (the game's rock material is world-aligned, but a UV set keeps importers happy).
bm = bmesh.new()
bm.from_mesh(mound.data)
uv = bm.loops.layers.uv.new("UVMap")
for f in bm.faces:
    n = f.normal
    axis = max(range(3), key=lambda k: abs(n[k]))
    for loop in f.loops:
        c = loop.vert.co
        loop[uv].uv = ((c.y, c.z), (c.x, c.z), (c.x, c.y))[axis]
        loop[uv].uv /= 4.0
bm.to_mesh(mound.data)
bm.free()
mat = bpy.data.materials.new("Rock")
mound.data.materials.append(mat)
v = [p.co for p in mound.data.vertices]
lo = Vector((min(c.x for c in v), min(c.y for c in v), min(c.z for c in v)))
hi = Vector((max(c.x for c in v), max(c.y for c in v), max(c.z for c in v)))
print(f"cave seed {seed}: {len(mound.data.polygons)} faces, bounds {tuple(round(c, 1) for c in lo)} .. {tuple(round(c, 1) for c in hi)}")

if preview:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.show_cavity = True
    scene.display.shading.light = "STUDIO"
    scene.render.resolution_x, scene.render.resolution_y = 800, 600
    scene.world = bpy.data.worlds.new("w")
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    shots = [((48, -22, 14), (0, 0, 3)),       # outside, front three-quarter: the entrance
             ((-40, 30, 26), (0, 0, 2)),       # outside, from behind
             ((5, 0, 2.2), (-10, 0, 3))]       # inside the chamber, looking back from the tunnel side
    for i, (loc, target) in enumerate(shots):
        cam.location = loc
        cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        cam.data.lens = 18 if i == 2 else 35
        scene.render.filepath = preview.replace(".png", f"_{i}.png")
        bpy.ops.render.render(write_still=True)

bpy.ops.object.select_all(action="DESELECT")
mound.select_set(True)
bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE",
                         axis_forward="X", axis_up="Z", add_leaf_bones=False, bake_anim=False)
print("exported", out)
