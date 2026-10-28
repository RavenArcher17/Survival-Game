# Blender (headless): measure the cave meshes (cave_gen.py output) for the dressing generator. For each variant, every
# surface crossing on vertical lines over a 1 m grid in the cave's local space (metres, top down), so
# Tools/terrain/dressing.mjs can tell where a cave is open air, how high its roof is over the tunnel, and where its walls
# meet the ground, and place each cave so its tunnel clears the landscape and its walls reach below it.
#   blender -b -P cave_measure.py -- <out.json> <cave_a.fbx> <cave_b.fbx> <cave_c.fbx>
# Axes: the FBX's +X (the tunnel, toward the entrance) and +Z stay; Unreal's FBX import flips Y, so y is negated here to
# give Unreal local space.
import bpy, sys, json
from mathutils import Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index("--") + 1:]
out = args[0]
variants = []
for path in args[1:]:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=path)
    obj = [o for o in bpy.context.scene.objects if o.type == "MESH"][0]
    me = obj.evaluated_get(bpy.context.evaluated_depsgraph_get()).to_mesh()
    verts = [obj.matrix_world @ v.co for v in me.vertices]
    bvh = BVHTree.FromPolygons(verts, [list(p.vertices) for p in me.polygons])
    lo = [min(v[k] for v in verts) for k in range(3)]
    hi = [max(v[k] for v in verts) for k in range(3)]
    x0, x1 = int(lo[0]) - 1, int(hi[0]) + 2
    y0, y1 = int(-hi[1]) - 1, int(-lo[1]) + 2
    grid = []
    for x in range(x0, x1):
        row = []
        for y in range(y0, y1):
            zs, o = [], Vector((x, -y, hi[2] + 5.0))
            while len(zs) < 8:
                loc, _, _, _ = bvh.ray_cast(o, Vector((0, 0, -1)), 100.0)
                if loc is None:
                    break
                zs.append(round(loc.z, 1))
                o = loc + Vector((0, 0, -0.01))
            row.append(zs)
        grid.append(row)
    variants.append({"name": path.replace("\\", "/").split("/")[-1].split(".")[0], "x0": x0, "y0": y0, "grid": grid,
                     "bounds": [[round(c, 1) for c in lo], [round(c, 1) for c in hi]]})
    print(f"measured {variants[-1]['name']}: x {x0}..{x1 - 1}, y {y0}..{y1 - 1}")
json.dump({"_comment": "Made by Tools/art/cave_measure.py from the cave FBXs (cave_gen.py). Per variant: grid[x - x0][y - y0] = "
           "surface crossings (m, top down) on the vertical line at local (x, y) m, Unreal axes.", "variants": variants},
          open(out, "w"), separators=(",", ":"))
print("wrote", out)
