"""Repairs two hero materials that don't compile on UE 5.8, in place (the lab script that built them isn't in the repo):

  M_WR_Armor      its HLSL (Custom) nodes read the vertex colour as float4 (VC.r, VC.g, VC.b, VC.a), but a Vertex Color
                  node's main output is RGB only on 5.8 ("vector swizzle 'a' is out of bounds"). Each such input now
                  gets Append(RGB, the node's own A output): the same four channels.
  M_WR_HairCards  the world-position-offset node (path lift / stir along the card, hero.md 2.6) read the hair's U from Hair
                  Attributes, which exists only in the pixel shader ("no matching function MaterialExpressionGetHairUV").
                  It now reads the card's texture U (TexCoord 0, root to tip on MetaHuman hair cards) instead; the
                  pixel-side uses of Hair Attributes are unchanged.

Run headless (UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>); safe to run again (it finds nothing to do).
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
log = []


def inputs_of(m, e):
    return list(mel.get_inputs_for_material_expression(m, e))


def custom_names(e):
    return [ci.get_editor_property("input_name") for ci in e.get_editor_property("inputs")]


def place(m, cls, near, dx=-260, dy=0):
    x = near.get_editor_property("material_expression_editor_x") + dx
    y = near.get_editor_property("material_expression_editor_y") + dy
    return mel.create_material_expression(m, cls, x, y)


def connect(a, out, b, pin):
    if not mel.connect_material_expressions(a, out, b, pin):
        raise RuntimeError(f"couldn't connect {a.get_name()}:{out!r} -> {b.get_name()}:{pin!r}")


# ---------------------------------------------------------------- the armour
armor = unreal.load_asset("/Game/Wildreach/Characters/Hero/Outfits/Armor/M_WR_Armor")
fixed = 0
for e in mel.get_material_expressions(armor):
    if not isinstance(e, unreal.MaterialExpressionCustom):
        continue
    names = custom_names(e)
    for i, src in enumerate(inputs_of(armor, e)):
        if isinstance(src, unreal.MaterialExpressionVertexColor) and i < len(names):
            rgba = place(armor, unreal.MaterialExpressionAppendVector, e, -220, 40 * i)
            connect(src, "", rgba, "A")
            connect(src, "A", rgba, "B")
            connect(rgba, "", e, str(names[i]))
            fixed += 1
log.append(f"M_WR_Armor: {fixed} vertex colour inputs now RGBA")
if fixed:
    mel.recompile_material(armor)
    eal.save_loaded_asset(armor)

# ---------------------------------------------------------------- the hair cards
cards = unreal.load_asset("/Game/Wildreach/Characters/Hero/Materials/M_WR_HairCards")
fixed = 0
wpo = mel.get_material_property_input_node(cards, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


def upstream(m, node, seen):
    """Every expression feeding node (itself included)."""
    if node is None or node.get_name() in seen:
        return []
    seen.add(node.get_name())
    out = [node]
    for s in inputs_of(m, node):
        out += upstream(m, s, seen)
    return out


# The material goes through Make Material Attributes, so the offset's own pin can't be asked for: the lift is the HLSL node
# reading Hair Attributes (the only Custom node in it), and the masks of the hair UV that feed the offset with it.
offset_side = upstream(cards, wpo, set()) if wpo else [e for e in mel.get_material_expressions(cards) if isinstance(e, unreal.MaterialExpressionCustom)]
for e in offset_side:
    if isinstance(e, unreal.MaterialExpressionComponentMask):
        # A mask of the hair's UV (r = U, g = V): the card's own UV in its place.
        if any(isinstance(s, unreal.MaterialExpressionHairAttributes) for s in inputs_of(cards, e)):
            uv = place(cards, unreal.MaterialExpressionTextureCoordinate, e, -260, 0)
            uv.set_editor_property("coordinate_index", 0)
            connect(uv, "", e, "")
            fixed += 1
        continue
    if not isinstance(e, unreal.MaterialExpressionCustom):
        continue
    names = custom_names(e)
    for i, src in enumerate(inputs_of(cards, e)):
        if isinstance(src, unreal.MaterialExpressionHairAttributes) and i < len(names):
            uv = place(cards, unreal.MaterialExpressionTextureCoordinate, e, -420, 40 * i)
            uv.set_editor_property("coordinate_index", 0)
            u = place(cards, unreal.MaterialExpressionComponentMask, e, -220, 40 * i)
            u.set_editor_property("r", True)
            for c in ("g", "b", "a"):
                u.set_editor_property(c, False)
            connect(uv, "", u, "")
            connect(u, "", e, str(names[i]))
            fixed += 1
log.append(f"M_WR_HairCards: {fixed} vertex-stage hair U inputs now the card's texture U")
if fixed:
    mel.recompile_material(cards)
    eal.save_loaded_asset(cards)

for line in log:
    unreal.log("[Wildreach] " + line)
open(r"E:\Dev\Wildreach\.lane-tmp\fix_hero.txt", "w").write("\n".join(log))
