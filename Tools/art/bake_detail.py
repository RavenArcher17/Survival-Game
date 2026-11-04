# Blender (headless): bake surface detail into a rigged creature's texture (image models come out smooth as soap):
# scales, feathers or fur as a tangent-space normal map, with the grooves shaded into a copy of its colour texture.
#   blender -b -P bake_detail.py -- <rigged.fbx> <out_prefix> --kind scales|feathers|fur [--size 0.025] [--depth 1.0]
#       [--res 2048] [--aurora]
# --size is one scale / feather / tuft across, as a share of the creature's height. --aurora (Aurora fox) softens the
# texture's hard green patches back to pale fur and lays a soft aurora (teal, blue, violet) over its back and tail.
# -> <out_prefix>_color.png and <out_prefix>_normal.png (OpenGL green: flip it on import to Unreal), on the model's UVs.
import bpy, sys
import numpy as np

args = sys.argv[sys.argv.index("--") + 1:]
src, out = args[0], args[1]


def opt(name, default):
    return type(default)(args[args.index(name) + 1]) if name in args else default


kind = opt("--kind", "scales")
size = opt("--size", {"scales": 0.025, "feathers": 0.05, "fur": 0.012}[kind])
depth = opt("--depth", 1.0)
res = opt("--res", 2048)
aurora = "--aurora" in args

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
me = next(o for o in bpy.data.objects if o.type == "MESH")
for m in list(me.modifiers):
    me.modifiers.remove(m)  # the rest pose
img = next((n.image for m in me.data.materials if m and m.use_nodes for n in m.node_tree.nodes
            if n.type == "TEX_IMAGE" and n.image), None)
if img is None:
    raise SystemExit("no colour texture on " + src)
co = np.array([v.co[:] for v in me.data.vertices])
lo, hi = co.min(0), co.max(0)
ext = np.maximum(hi - lo, 1e-6)
H = float(ext[2])

mat = bpy.data.materials.new("Bake")
mat.use_nodes = True
nt = mat.node_tree
nt.nodes.clear()
N, L = nt.nodes.new, nt.links.new


def math_node(op, a, b=None, clamp=False):
    n = N("ShaderNodeMath")
    n.operation = op
    n.use_clamp = clamp
    for i, x in enumerate((a, b)):
        if x is None:
            continue
        if isinstance(x, (int, float)):
            n.inputs[i].default_value = x
        else:
            L(x, n.inputs[i])
    return n.outputs[0]


def ramp(x, a, b):
    """smoothstep(a, b, x)"""
    n = N("ShaderNodeMapRange")
    n.interpolation_type = "SMOOTHSTEP"
    L(x, n.inputs["Value"])
    n.inputs["From Min"].default_value, n.inputs["From Max"].default_value = a, b
    return n.outputs["Result"]


tc = N("ShaderNodeTexCoord")
tex = N("ShaderNodeTexImage")
tex.image = img
L(tc.outputs["UV"], tex.inputs["Vector"])
base = tex.outputs["Color"]
# Pattern space: the model's own coordinates in creature heights, so a scale is the same size all over it.
pat = N("ShaderNodeMapping")
L(tc.outputs["Object"], pat.inputs["Vector"])
stretch = {"scales": (1.0, 1.0, 1.0), "feathers": (0.4, 1.0, 1.0), "fur": (0.3, 1.0, 1.0)}[kind]  # long along the body
pat.inputs["Scale"].default_value = [s / (size * H) for s in stretch]
if kind in ("scales", "feathers"):
    cell = N("ShaderNodeTexVoronoi")
    cell.feature = "DISTANCE_TO_EDGE"
    cell.inputs["Randomness"].default_value = 0.85
    L(pat.outputs["Vector"], cell.inputs["Vector"])
    centre = N("ShaderNodeTexVoronoi")  # distance to the cell's middle: each scale domes, each feather lies over the next
    centre.inputs["Randomness"].default_value = 0.85
    L(pat.outputs["Vector"], centre.inputs["Vector"])
    edge = ramp(cell.outputs["Distance"], 0.0, 0.14 if kind == "scales" else 0.2)  # 0 in the groove, 1 on the face
    dome = math_node("SUBTRACT", 1.0, centre.outputs["Distance"], clamp=True)
    height = math_node("MULTIPLY", edge, math_node("ADD", 0.55, math_node("MULTIPLY", dome, 0.45)))
    fine = N("ShaderNodeTexNoise")  # pitting on scales, barbs on feathers
    fine.inputs["Scale"].default_value = 9.0
    fine.inputs["Detail"].default_value = 6.0
    L(pat.outputs["Vector"], fine.inputs["Vector"])
    height = math_node("ADD", height, math_node("MULTIPLY", fine.outputs["Fac"], 0.12))
    shade = math_node("ADD", 0.55 if kind == "scales" else 0.75, math_node("MULTIPLY", edge, 0.45 if kind == "scales" else 0.25))
else:
    strands = N("ShaderNodeTexNoise")
    strands.inputs["Scale"].default_value = 1.0
    strands.inputs["Detail"].default_value = 10.0
    strands.inputs["Roughness"].default_value = 0.65
    L(pat.outputs["Vector"], strands.inputs["Vector"])
    height = strands.outputs["Fac"]
    shade = math_node("ADD", 0.78, math_node("MULTIPLY", ramp(height, 0.3, 0.7), 0.3))
col_node = N("ShaderNodeMix")
col_node.data_type = "RGBA"
col_node.blend_type = "MULTIPLY"
col_node.inputs["Factor"].default_value = 1.0
L(base, col_node.inputs["A"])
L(shade, col_node.inputs["B"])
colour = col_node.outputs["Result"]

if aurora:
    # Where the old texture is green (hue 0.25-0.55, saturated): back to pale cream fur, its shading kept.
    hsv = N("ShaderNodeSeparateColor")
    hsv.mode = "HSV"
    L(base, hsv.inputs["Color"])
    green = math_node("MULTIPLY", math_node("MULTIPLY", ramp(hsv.outputs[0], 0.2, 0.28), math_node("SUBTRACT", 1.0, ramp(hsv.outputs[0], 0.52, 0.6))),
                      ramp(hsv.outputs[1], 0.15, 0.35))
    cream = N("ShaderNodeMix")
    cream.data_type = "RGBA"
    L(green, cream.inputs["Factor"])
    L(colour, cream.inputs["A"])
    pale = N("ShaderNodeCombineColor")
    for i, c in enumerate((0.93, 0.89, 0.82)):
        L(math_node("MULTIPLY", hsv.outputs[2], c), pale.inputs[i])
    L(pale.outputs[0], cream.inputs["B"])
    # The aurora: soft curtains over the upper body and the tail, teal at the shoulders to violet at the tail tip.
    unit = N("ShaderNodeMapping")
    L(tc.outputs["Object"], unit.inputs["Vector"])
    unit.inputs["Scale"].default_value = [1.0 / e for e in ext]
    unit.inputs["Location"].default_value = [-l / e for l, e in zip(lo, ext)]
    xyz = N("ShaderNodeSeparateXYZ")
    L(unit.outputs["Vector"], xyz.inputs["Vector"])
    wisp = N("ShaderNodeTexNoise")
    wisp.inputs["Scale"].default_value = 3.0
    wisp.inputs["Detail"].default_value = 2.0
    L(unit.outputs["Vector"], wisp.inputs["Vector"])
    band = math_node("ADD", xyz.outputs["X"], math_node("MULTIPLY", math_node("SUBTRACT", wisp.outputs["Fac"], 0.5), 0.35))
    cr = N("ShaderNodeValToRGB")
    els = cr.color_ramp.elements
    els[0].position, els[0].color = 0.05, (0.62, 0.38, 1.0, 1)   # tail tip: violet
    els[1].position, els[1].color = 0.95, (0.2, 1.0, 0.8, 1)     # shoulders: teal
    mid = els.new(0.5)
    mid.color = (0.3, 0.7, 1.0, 1)                                # blue between
    L(band, cr.inputs["Fac"])
    over = math_node("MULTIPLY", ramp(xyz.outputs["Z"], 0.2, 0.5), math_node("ADD", 0.55, math_node("MULTIPLY", wisp.outputs["Fac"], 0.45)))
    over = math_node("MAXIMUM", over, math_node("MULTIPLY", math_node("SUBTRACT", 1.0, ramp(xyz.outputs["X"], 0.1, 0.3)), 0.8))
    over = math_node("MULTIPLY", over, math_node("SUBTRACT", 1.0, ramp(xyz.outputs["X"], 0.68, 0.8)))  # not on the face
    # Laid over the pale fur as colour (keeping the fur's own light and shade), strongest along the spine and tail.
    lit = N("ShaderNodeMix")
    lit.data_type = "RGBA"
    lit.blend_type = "MULTIPLY"
    lit.inputs["Factor"].default_value = 1.0
    L(cr.outputs["Color"], lit.inputs["A"])
    L(math_node("MULTIPLY", hsv.outputs[2], 1.15), lit.inputs["B"])
    tint = N("ShaderNodeMix")
    tint.data_type = "RGBA"
    L(math_node("MULTIPLY", over, 0.85), tint.inputs["Factor"])
    L(cream.outputs["Result"], tint.inputs["A"])
    L(lit.outputs["Result"], tint.inputs["B"])
    colour = tint.outputs["Result"]

bump = N("ShaderNodeBump")
bump.inputs["Strength"].default_value = min(1.0, 0.6 * depth)
bump.inputs["Distance"].default_value = size * H * 0.25 * depth
L(height, bump.inputs["Height"])
bsdf = N("ShaderNodeBsdfPrincipled")
L(bump.outputs["Normal"], bsdf.inputs["Normal"])
emit = N("ShaderNodeEmission")
L(colour, emit.inputs["Color"])
outn = N("ShaderNodeOutputMaterial")
me.data.materials.clear()
me.data.materials.append(mat)

sc = bpy.context.scene
sc.render.engine = "CYCLES"
sc.cycles.device = "CPU"
sc.cycles.samples = 1
sc.render.bake.margin = 8
for o in bpy.data.objects:
    o.select_set(o is me)  # bake the mesh alone (the import leaves its armature selected)
bpy.context.view_layer.objects.active = me


def bake(name, btype, colorspace):
    target = bpy.data.images.new(name, res, res, alpha=False)
    target.colorspace_settings.name = colorspace
    node = N("ShaderNodeTexImage")
    node.image = target
    for n in nt.nodes:
        n.select = False
    node.select = True
    nt.nodes.active = node
    bpy.ops.object.bake(type=btype, normal_space="TANGENT") if btype == "NORMAL" else bpy.ops.object.bake(type=btype)
    target.filepath_raw = f"{out}_{name}.png"
    target.file_format = "PNG"
    target.save()
    print("BAKED", target.filepath_raw)


L(bsdf.outputs["BSDF"], outn.inputs["Surface"])
bake("normal", "NORMAL", "Non-Color")
L(emit.outputs["Emission"], outn.inputs["Surface"])
bake("color", "EMIT", "sRGB")
