# Blender (headless): the mesh of a rigged FBX in its rest pose, textured, as a GLB, to re-rig a model whose legs an
# earlier rig already fixed:  blender -b -P fbx_to_glb.py -- out\cinderfang.fbx out\cinderfang_4leg.glb
import bpy, sys
argv = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=argv[0])
for o in list(bpy.data.objects):
    if o.type == "MESH":
        for m in list(o.modifiers):
            o.modifiers.remove(m)
        o.parent = None
for o in list(bpy.data.objects):
    if o.type != "MESH":
        bpy.data.objects.remove(o)
bpy.ops.export_scene.gltf(filepath=argv[1], export_format="GLB", export_animations=False, export_skins=False)
print("GLB written", argv[1])
