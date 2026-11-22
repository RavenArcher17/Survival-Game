"""Building materials for metal, the 12 biome sets and build mode (build-building, cooks/build-building.md). Safe to re-run.
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="Tools/make_build_materials_tinted.py"

Shares the graph helpers of make_build_materials.py (read from it, up to its first material) without rebuilding the wood
and stone materials.

  M_BuildTinted   every tinted detail block: colour, metalness, roughness, glow and surface pattern per instance.
  M_BuildOverlay  the see-through stability colour build mode lays over pieces.
"""
import os
import unreal

_here = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(_here, "make_build_materials.py"), encoding="utf-8").read()
exec(_src[: _src.index("# ================================================================ M_BuildWood")])

# ================================================================ M_BuildTinted (build-building: metal and the 12 biome sets)
# One material for every tinted block (WRBuildLook EWRLookMat::Tinted): per-instance custom data
#   0 shade (0..1)  1 grain axis (0 X, 1 Y, 2 Z)  2 brightness  3..5 colour (linear RGB)  6 metallic  7 roughness
#   8 glow (emissive x colour)  9 pattern: 0 mottled stone, 1 wood grain, 2 brushed metal, 3 smooth (glass, glaze),
#   4 plaster, 5 clay pantiles, 6 thatch, 7 wooden shingles, 8 slate (the roofs, 11-22), 9 cloth (canvas, wool), 10 ice
# So 13 building tiers and every set's furniture share one material and one draw per shape.
begin("/Game/Building/M_BuildTinted")
shade, axis_raw, dim = custom(0, 0.5), custom(1, 0.0), custom(2, 1.0)
axis, rw, arc = round_info(axis_raw)
rgb = op(unreal.MaterialExpressionAppendVector, op(unreal.MaterialExpressionAppendVector, custom(3, 0.5), custom(4, 0.5)), custom(5, 0.5))
metal, rough, glow, pattern = custom(6, 0.0), custom(7, 0.85), custom(8, 0.0), custom(9, 0.0)
wx, wy, wz = world_xyz()
# each face mapped in the block's own frame (ui along its grain axis, vi across, in cm; 11-22: from the world's axes a
# turned block was mapped sheared); big blocks (a wall, a floor) draw their joints: stone courses, floor boards
ti, bi, ui, vi = block_frame(axis, (rw, arc))
big = block_big()
seed = mul(shade, k=7.0)
grain_n = noise(op(unreal.MaterialExpressionAppendVector, add(mul(ui, k=1.0 / 150.0), seed), mul(vi, k=1.0 / 5.0)))
brush_n = noise(op(unreal.MaterialExpressionAppendVector, add(mul(ui, k=1.0 / 400.0), seed), mul(vi, k=1.0 / 0.8)))
mottle_n = noise(op(unreal.MaterialExpressionAppendVector, add(mul(ui, k=1.0 / 70.0), seed), mul(vi, k=1.0 / 70.0)))
# weights of the patterns (pattern is a whole number): 1 on its own pattern, 0 elsewhere
def w_of(k):
    return sat(unary(unreal.MaterialExpressionOneMinus, unary(unreal.MaterialExpressionAbs, add(pattern, k=-float(k)))))
w_mottle, w_grain, w_brush, w_smooth = w_of(0), w_of(1), w_of(2), w_of(3)
w_plaster, w_tile, w_thatch, w_shingle, w_slate, w_cloth, w_ice = w_of(4), w_of(5), w_of(6), w_of(7), w_of(8), w_of(9), w_of(10)
w_roof = add(add(add(w_tile, w_thatch), w_shingle), w_slate)
# (any other pattern number: plain, as smooth; a pattern with no weight at all would leave the normal zero, black)
w_none = sat(op(unreal.MaterialExpressionSubtract, None, add(add(add(add(w_mottle, w_grain), add(w_brush, w_smooth)), add(w_plaster, w_roof)), add(w_of(9), w_of(10))), const_a=1.0))
# Real surfaces under the tint (11-22, "everything as good as the caves"): the stone pattern SimpleRocks' rock, the wood
# the scanned boards when the pack is there (texture parameters set at run time, FWRBuildLook::Material; else the old
# noise), the metal hammered iron (the outfits' texture, in git); each lends its light and dark as the detail and its
# relief to the normal. Smooth (glass, glaze) stays plain. Plaster (4) is limewashed render: faint mottling and stains.
# The roofs (5 clay pantiles, 6 thatch, 7 wooden shingles, 8 slate) are drawn from the face itself: courses run level
# (by height), the tiles across them along the face's level direction, staggered row by row, each tile its own shade,
# the overlap's shadow under each course; on a roof block's upright faces (a gable's end) the plaster instead.
nrm = expr(unreal.MaterialExpressionVertexNormalWS)
lumw = const3((0.3, 0.59, 0.11))
def lum(c):
    d = expr(unreal.MaterialExpressionDotProduct)
    link(c, d, "A")
    link(lumw, d, "B")
    return d
uv_r = append(add(mul(ui, k=1.0 / 150.0), mul(shade, k=3.1)), add(mul(vi, k=1.0 / 150.0), mul(shade, k=1.7)))
rock_c = mask(tex("/Game/SimpleRocks/Master_Materials/Textures/TX_Rock_08_D", uv_r), "r", "g", "b")
rock_n = tex("/Game/SimpleRocks/Master_Materials/Textures/TX_Rock_08_N", uv_r)
uv_w = append(add(mul(vi, k=1.0 / 140.0), mul(shade, k=5.37)), add(mul(ui, k=1.0 / 140.0), mul(shade, k=2.71)))
wood_c = power(mask(tex_param("WoodColor", GRID, uv_w), "r", "g", "b"), 2.2)
wood_n = tex_param("WoodNormal", FLAT, uv_w)
scan = scalar_p("Scan", 0.0)
uv_i = append(mul(ui, k=1.0 / 60.0), mul(vi, k=1.0 / 60.0))
iron_c = mask(tex("/Game/Wildreach/Characters/Hero/Outfits/Armor/T_HammeredIron_BC", uv_i), "r", "g", "b")
iron_n = tex("/Game/Wildreach/Characters/Hero/Outfits/Armor/T_HammeredIron_N", uv_i)
VAR = "/Game/MWLandscapeAutoMaterial/Textures/Extra/TEX_MWAM_Variation_msk"
d_rock = sat(mul(add(lum(rock_c), k=-0.08), k=2.6))
d_wood = lerp(grain_n, sat(mul(add(lum(wood_c), k=-0.04), k=2.2)), scan)
d_iron = sat(mul(add(lum(iron_c), k=-0.08), k=2.4))
# plaster: broad mottling, a little of the rock's grain, stains
pl_macro = mask(tex(VAR, append(mul(ui, k=1.0 / 260.0), mul(vi, k=1.0 / 260.0))), "r")
pl_stain = mask(tex(VAR, append(mul(ui, k=1.0 / 90.0), mul(wz, k=1.0 / 160.0))), "g")
pl_streak = mask(tex(VAR, append(mul(ui, k=1.0 / 22.0), mul(wz, k=1.0 / 420.0))), "b")
d_plaster = sat(add(add(mul(pl_macro, k=0.5), mul(d_rock, k=0.22)), add(add(mul(pl_stain, k=-0.2), mul(sat(mul(add(pl_streak, k=-0.55), k=3.0)), k=-0.22)), const(0.4))))
# cloth (9: canvas, wool, linen): the outfits' linen weave
uv_c = append(mul(ui, k=1.0 / 28.0), mul(vi, k=1.0 / 28.0))
cloth_c = mask(tex("/Game/Wildreach/Characters/Hero/Outfits/Common/T_Linen_BC", uv_c), "r", "g", "b")
cloth_n = tex("/Game/Wildreach/Characters/Hero/Outfits/Common/T_Linen_N", uv_c)
d_cloth = sat(add(mul(add(lum(cloth_c), k=-0.3), k=1.6), const(0.5)))
# glacier ice (10): level strata by height bent by noise, darker knots; its scallops from the stones' relief turned
# inside out; snow on what faces up (then ice_snow lifts the colour to snow's below)
ice_warp = mask(tex(VAR, append(mul(add(wx, wy), k=1.0 / 1500.0), mul(wz, k=1.0 / 1500.0))), "r")
ice_f = frac(add(mul(wz, k=1.0 / 95.0), mul(ice_warp, k=1.4)))
ice_strata = sat(mul(add(unary(unreal.MaterialExpressionAbs, add(mul(ice_f, k=2.0), k=-1.0)), k=-0.25), k=2.0))
ice_knots = mask(tex(VAR, append(mul(add(wx, wy), k=1.0 / 420.0), mul(wz, k=1.0 / 420.0))), "g")
d_ice = sat(add(mul(ice_strata, k=0.45), add(mul(sat(mul(add(ice_knots, k=-0.6), k=3.0)), k=-0.3), const(0.5))))
uv_s = append(mul(ui, k=1.0 / 340.0), mul(vi, k=1.0 / 340.0))
ice_n = tex("/Game/MWLandscapeAutoMaterial/Textures/Ground/TEX_MWAM_Stones_nrm", uv_s)
ice_snow = mul(w_ice, sat(mul(add(mask(nrm, "b"), k=-0.6), k=4.0)))
n = add(add(add(mul(d_rock, w_mottle), mul(d_wood, w_grain)), mul(mul(d_iron, w_brush), k=0.8)),
        add(add(mul(d_plaster, w_plaster), mul(w_roof, const(0.5))), add(mul(d_cloth, w_cloth), mul(d_ice, w_ice))))
# ---- roofs
wp = expr(unreal.MaterialExpressionWorldPosition)
up = const3((0.0, 0.0, 1.0))
cr = expr(unreal.MaterialExpressionCrossProduct)
link(up, cr, "A")
link(nrm, cr, "B")
hz = normalized(add(cr, const3((0.0001, 0.0, 0.0))))   # along the face, level
cr2 = expr(unreal.MaterialExpressionCrossProduct)
link(nrm, cr2, "A")
link(hz, cr2, "B")
slope_d = cr2                                          # along the face, up/down the slope
dot_u = expr(unreal.MaterialExpressionDotProduct)
link(wp, dot_u, "A")
link(hz, dot_u, "B")
ru = lerp(dot_u, arc, rw)                              # cm across the roof (round it on a cone)
sloped = sat(mul(add(unary(unreal.MaterialExpressionAbs, mask(nrm, "b")), k=-0.2), k=8.0))
def course(row_h, width, jitter):
    """(row, across 0..1 in the tile, up 0..1 in the course, the tile's own 0..1) for courses row_h cm high."""
    rv = mul(wz, k=1.0 / row_h)
    row = floor(rv)
    tv = frac(rv)
    rh = hash1(mul(row, k=12.9898))
    tu_raw = mul(add(ru, mul(add(mul(frac(mul(row, k=0.5)), k=width), mul(rh, k=jitter)), k=1.0)), k=1.0 / width)
    tile = hash1(add(mul(row, k=7.13), mul(floor(tu_raw), k=3.71)))
    return row, frac(tu_raw), tv, tile
sine = lambda x, period: unary(unreal.MaterialExpressionSine, x, period=period)
# clay pantiles: 26 cm wide, courses 14 cm high, each a roll across (light on its crown, dark in its trough)
_, t_u, t_v, t_h = course(14.0, 26.0, 0.0)
roll = sine(t_u, 2.0)                                   # sin(pi * u): 0 at the edges, 1 at the crown
shadow = op(unreal.MaterialExpressionSubtract, None, mul(sat(mul(add(t_v, k=-0.78), k=4.5)), k=0.55), const_a=1.0)
tile_mul = mul(mul(lerp(const(0.78), const(1.16), t_h), lerp(const(0.72), const(1.08), roll)), shadow)
cosroll = sine(add(t_u, k=0.5), 2.0)                   # cos(pi * u)
tile_nrm = add(nrm, mul(hz, mul(cosroll, k=-0.45)))
# slate: 30 cm wide, courses 9 cm, flat, thin dark joints
_, s_u, s_v, s_h = course(9.0, 30.0, 11.0)
s_gap = mul(edge(s_v, 0.07), edge(s_u, 0.025))
slate_mul = mul(mul(lerp(const(0.7), const(1.2), s_h), lerp(const(0.35), const(1.0), s_gap)), op(unreal.MaterialExpressionSubtract, None, mul(sat(mul(add(s_v, k=-0.85), k=6.0)), k=0.35), const_a=1.0))
# wooden shingles: 17 cm wide, courses 12 cm, ragged by course, a grain running down the slope, silvered and mossy here and there
_, h_u, h_v, h_h = course(12.0, 17.0, 9.0)
h_grain = mask(tex(VAR, append(mul(ru, k=1.0 / 3.0), mul(wz, k=1.0 / 70.0))), "g")
h_gap = mul(edge(h_u, 0.05), edge(h_v, 0.05))
shingle_mul = mul(mul(mul(lerp(const(0.7), const(1.15), h_h), lerp(const(0.82), const(1.1), h_grain)), lerp(const(0.3), const(1.0), h_gap)),
                  op(unreal.MaterialExpressionSubtract, None, mul(sat(mul(add(h_v, k=-0.8), k=5.0)), k=0.45), const_a=1.0))
# thatch: straw strands down the slope in layers 26 cm deep, each layer's lower edge a soft step
strands = mask(tex(VAR, append(mul(ru, k=1.0 / 2.2), mul(wz, k=1.0 / 38.0))), "g")
strands2 = mask(tex(VAR, append(mul(ru, k=1.0 / 0.9), mul(wz, k=1.0 / 22.0))), "b")
th_rv = mul(wz, k=1.0 / 26.0)
th_v = frac(th_rv)
th_macro = mask(tex(VAR, append(mul(ru, k=1.0 / 400.0), mul(wz, k=1.0 / 300.0))), "r")
thatch_mul = mul(mul(lerp(const(0.55), const(1.2), add(mul(strands, k=0.6), mul(strands2, k=0.4))), lerp(const(0.8), const(1.1), th_macro)),
                 op(unreal.MaterialExpressionSubtract, None, mul(sat(mul(add(th_v, k=-0.82), k=5.5)), k=0.4), const_a=1.0))
thatch_nrm = add(nrm, mul(hz, mul(add(strands2, k=-0.5), k=0.8)))
roof_mul = add(add(mul(tile_mul, w_tile), mul(slate_mul, w_slate)), add(mul(shingle_mul, w_shingle), mul(thatch_mul, w_thatch)))
# on upright faces of a roof block (a gable's end) the roof patterns give way to the plaster's look
roof_mul = lerp(lerp(const(1.0), d_plaster, w_roof), roof_mul, sloped)
roof_mul = lerp(const(1.0), roof_mul, w_roof)
roof_nrm = add(add(mul(tile_nrm, w_tile), mul(thatch_nrm, w_thatch)), mul(nrm, add(w_shingle, w_slate)))
roof_nrm = lerp(nrm, roof_nrm, sloped)
plaster_nrm = bumped(nrm, ti, bi, rock_n, 0.18)
normal = normalized(add(add(add(mul(bumped(nrm, ti, bi, rock_n, 0.8), w_mottle), mul(bumped(nrm, bi, ti, wood_n, 0.8), mul(w_grain, scan))),
                            mul(nrm, mul(w_grain, op(unreal.MaterialExpressionSubtract, None, scan, const_a=1.0)))),
                        add(add(add(mul(bumped(nrm, ti, bi, iron_n, 0.6), w_brush), mul(nrm, w_smooth)), add(mul(plaster_nrm, w_plaster), mul(roof_nrm, w_roof))),
                            add(add(mul(bumped(nrm, ti, bi, cloth_n, 0.5), w_cloth), mul(nrm, w_none)), mul(bumped(nrm, ti, bi, ice_n, -0.25), w_ice)))))
amount = lerp(const(0.45), const(0.08), w_smooth)
tone = sat(add(add(mul(shade, k=0.35), mul(n, amount)), k=0.3))
base = mul(mul(mul(rgb, lerp(const(0.72), const(1.18), tone)), dim), roof_mul)
# boards 18 cm wide and 2.4 m long along the block's grain, ends staggered
bp_v = mul(vi, k=1.0 / 18.0)
bp_row = floor(bp_v)
bp_u = mul(add(ui, mul(hash1(mul(bp_row, k=12.9898)), k=240.0)), k=1.0 / 240.0)
bp_id = hash1(add(mul(bp_row, k=7.13), mul(floor(bp_u), k=3.71)))
bp_seam = mul(edge(frac(bp_v), 0.035), edge(frac(bp_u), 0.005))
boards = lerp(const(1.0), mul(lerp(const(0.38), const(1.0), bp_seam), lerp(const(0.8), const(1.16), bp_id)), mul(big, w_grain))
# stone courses 45 cm high, blocks 90 cm, every other course shifted, on the world's courses (they run on across blocks)
cs_u, cs_v = courses()
cs_u = lerp(cs_u, arc, rw)
cs_pv = mul(cs_v, k=1.0 / 36.0)
cs_row = floor(cs_pv)
cs_pu = op(unreal.MaterialExpressionDivide, add(cs_u, mul(hash1(mul(cs_row, k=12.9898)), k=90.0)), add(mul(hash1(mul(cs_row, k=3.17)), k=26.0), k=62.0))
cs_id = hash1(add(mul(cs_row, k=12.9898), mul(floor(cs_pu), k=78.233)))
cs_mortar = mul(edge(frac(cs_pv), 0.06), edge(frac(cs_pu), 0.03))
cs_bevel = mul(edge(frac(cs_pv), 0.2), edge(frac(cs_pu), 0.1))
ashlar = lerp(const(1.0), mul(mul(lerp(const(0.3), const(1.0), cs_mortar), lerp(const(0.74), const(1.0), cs_bevel)), lerp(const(0.86), const(1.1), cs_id)), mul(big, w_mottle))
# cloth: sewn panels (a seam every 90 cm along the piece, a little darker and puckered) and weathered blotches
cl_seam = mul(edge(frac(mul(ui, k=1.0 / 90.0)), 0.04), edge(frac(add(mul(ui, k=1.0 / 90.0), k=0.06)), 0.012))   # a 3.5 cm felled seam and its second stitch row
cl_stain = mask(tex(VAR, append(mul(ui, k=1.0 / 120.0), mul(vi, k=1.0 / 120.0))), "b")
canvas = lerp(const(1.0), mul(lerp(const(0.6), const(1.0), cl_seam), lerp(const(0.72), const(1.08), cl_stain)), w_cloth)
base = mul(mul(mul(base, boards), ashlar), canvas)
base = lerp(base, const3((0.66, 0.75, 0.84)), ice_snow)
# a roof seen from inside (its faces turned down): the boards under the tiles, dark timber, not the tiles' colour
under = mul(w_roof, sat(mul(add(mul(mask(nrm, "b"), k=-1.0), k=-0.2), k=5.0)))
# boards 22 cm wide down the slope, dark joints, a rafter every 90 cm (11-22: the underside read as rough stone)
ub_seam = edge(frac(mul(ru, k=1.0 / 22.0)), 0.05)
ub_raft = sat(mul(sub(const(0.12), frac(mul(ru, k=1.0 / 90.0))), k=40.0))
under_c = mul(mul(const3((0.07, 0.05, 0.035)), lerp(const(0.7), const(1.2), d_wood)), mul(lerp(const(0.45), const(1.0), ub_seam), lerp(const(1.0), const(1.5), ub_raft)))
base = lerp(base, under_c, under)
r_out = sat(add(rough, mul(add(n, k=-0.5), k=0.12)))
mat.set_editor_property("tangent_space_normal", False)
mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(r_out, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(mul(mul(rgb, glow), k=4.0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")

# ================================================================ M_BuildOverlay (build mode's stability colours)
# A see-through, unlit wash laid over a piece (UMeshComponent overlay material): "Color" by how much span it has left,
# brighter at grazing angles so edges read.
begin("/Game/Building/M_BuildOverlay")
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
colour = expr(unreal.MaterialExpressionVectorParameter, parameter_name="Color", default_value=unreal.LinearColor(0.2, 0.9, 0.3, 1.0))
fres = expr(unreal.MaterialExpressionFresnel, exponent=3.0, base_reflect_fraction=0.0)
mel.connect_material_property(mul(colour, k=1.6), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(sat(add(mul(fres, k=0.35), k=0.28)), "", unreal.MaterialProperty.MP_OPACITY)
mel.set_base_material_usage(mat, unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
unreal.log(f"[Wildreach] built {mat.get_name()} ({col[0]} expressions)")
