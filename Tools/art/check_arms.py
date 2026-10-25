# Blender (headless): are a humanoid's arms clear of its body? Slices the mesh at several heights and counts the
# separate shapes in each slice: arms clear of the torso show up as 3 (left arm, body, right arm).
#   blender -b -P check_arms.py -- <model.fbx|glb>
import bpy, sys, bmesh
import numpy as np

src = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
(bpy.ops.import_scene.fbx if src.lower().endswith(".fbx") else bpy.ops.import_scene.gltf)(filepath=src)
o = [x for x in bpy.context.scene.objects if x.type == "MESH"][0]
v = np.array([o.matrix_world @ p.co for p in o.data.vertices])
lo, hi = v.min(0), v.max(0)
H = hi[2] - lo[2]
cell = H / 120
for frac in (0.80, 0.72, 0.65, 0.58, 0.52):
    z = lo[2] + frac * H
    band = v[np.abs(v[:, 2] - z) < H * 0.012][:, 0]  # x positions of vertices in a thin horizontal band
    if len(band) == 0:
        continue
    occ = np.unique(np.floor((band - lo[0]) / cell).astype(int))
    pieces, widths, start = 0, [], None
    for i, c in enumerate(occ):
        if i == 0 or c - occ[i - 1] > 2:  # a gap of more than 2 cells splits shapes
            if start is not None:
                widths.append(round((occ[i - 1] - start + 1) * cell, 2))
            start = c
            pieces += 1
    widths.append(round((occ[-1] - start + 1) * cell, 2))
    print(f"ARMS slice at {frac:.0%} height: {pieces} shape(s), widths {widths}")
