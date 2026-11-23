"""Generated meshes for the things the art packs don't have (11-22, Raven: "everything as good as the caves"). Safe to
re-run: each asset is made again in /Game/Art/Gen (in git: our own geometry, no pack's).
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="<full path>/Tools/art/make_meshes.py"
     WR_MESHES=basalt,obsidian (comma list) makes only those.

  SM_BasaltColumns_A..C   clusters of hexagonal basalt columns (Giant's Causeway style), heights stepped, tops a little
                          tilted and broken; about 3 x 3 m across, 5.6 m tall at the highest.
  SM_ObsidianBlade_A..C   clusters of thin faceted glass blades leaning out of a stub, 4.2 m tall at the highest.
  SM_SulfurCrystal        one small crystal: a squashed double point (the sulfur node's crust is covered in them).
  SM_Serac_A..D           tall leaning ice towers, each one prism of sheared faces with a slanted broken top, a lesser
                          block fused to it, chunks at its foot (round 4; they were stacked boxes).
  SM_Rubble_A..C          a patch of broken basalt about 4.4 m across: ~40 angular stones (5-45 cm), bigger and
                          thicker toward the middle, half sunk; Nanite (the Volcanic's lava_rubble dressing).
  SM_TentRidge_A          a ridge tent's canvas (the wake-up camps): sagging panels, a flared hem tucked into the ground,
                          the back gable closed, a triangular doorway in the front; inside faces of its own. X along the
                          ridge (the door at -X), 280 cm long, 290 across the hem, the ridge 150 cm up. M_BuildTinted
                          (cloth) as an instanced block (WRBuildLook EWRLookMesh::Tent).
  SM_IceArch_A            a natural ice bridge 72 m across, 13 m high, icicles under it (the Ice Arch); collides.
  SM_Pingo_A              an ice-cored mound 26 m across, 10.5 m high, a cracked crown, one flank broken open.
  SM_IceBlock_A..C        a block of glacier ice 4 m each way, top broken at a slant, faces split (the Crevasse Maze).
  SM_Hornito_A..B         a lava chimney of welded spatter, 20 m tall, an open throat (the Bellows Stacks, hornitos).
  The last places (round 4), drawn by FWRIsleSite::Shape / ShapeMat / ShapeKit:
  SM_GiantSkull_A, _Jaw_A the Fallen Smith's skull (5.6 m: braincase, brow, cheekbones, the sockets and the nose's opening
                          cut by booleans, teeth) and its lower jaw; SM_GiantVertebra_A, SM_GiantRib_A (whole) / _B
                          (snapped) / _C (the fallen piece, lying), SM_GiantLongBone_A (a 15 m thigh bone, scaled for the
                          shin and the arm), SM_GiantPelvis_A: parts merged by voxels, filled round where they meet.
  SM_Longboat_A           the Frozen Whaler: a clinker hull 19 m long, strakes lapped, ribs and thwarts inside, stems curled.
  SM_WispSpire_A, _Ribs_A Ignessa's spire (nine hexagonal drums, each turned 7 degrees, worn joints, a split crown) and its
                          three glassy ribs spiralling up it (their own mesh, for the obsidian).
  SM_Glasswall_A          a 52 m cliff of fused obsidian slabs leaning out, scalloped (conchoidal) faces, a plain patch for
                          the Mirror Room's door 8.5-14.5 m up.
  SM_QuarryFace_A         Warden's Quarry: an outcrop cut back in three benches scored by drill channels, a 15 m skirt.
  SM_CutBlock_A..B        blocks split from it: chisel grooves, wedge holes along one edge, corners knocked.
  SM_CragSoft_A..D        the same with snow and ice built up over them, every edge rounded (the Tundra's faces).
  SM_IceFace_A..D         a glacier's face 12 m across, up to 10 m tall: fluted top to bottom, stepped where its layers
                          end, its top rounding over (some with a cornice), its sides and top sunk back into the slope so
                          overlapping faces join (the Tundra's steep ice: ice_cliff).
  The town (round 8, drawn as blocks: each fits a 100 cm box like the engine shapes): SM_Awning_A (a stall's canvas
                          sagging between its posts, a scalloped valance), SM_Haystack_A (a beehive of hay),
                          SM_Rowboat_A (a clinker rowboat, thwarts and keel).
  The fields (round 8): SM_WheatClump_A..B (~50 stalks with ears, 0.9 m across, a metre tall), SM_BarleyClump_A..B
                          (nodding bearded ears), SM_Cabbage_A..B (a head in a rosette of cupped leaves).
  The coast (round 7): SM_Pebbles_A..C (a patch of sea-worn pebbles 3.4 m across), SM_Driftwood_A..C (drift logs 4.6,
                          3.3, 2.5 m: grooved along the grain, root plates or snapped ends, branch stubs), SM_Wrack_A..C
                          (the tide's strand line of weed 5.4 m long, kelp straps lying out of it).
  SM_Crag_A..D            jointed rock panels for steep faces (10 m across, up to 8.5 m tall, 4.8 m deep), laid against
                          the slope by the dressing (basalt_cliff, ice_cliff, crag_*): the ground's cliffs read as rock.
  SM_GolemRough_A         the quarry's second golem, half carved: a rough block 4.6 m long with a figure lying face up
                          out of its top (torso, head, shoulders, arms at its sides, hips), its legs still in the stone.
  Helpers: prism() (faceted columns, sharp edges), tube() (swept rings), ellipsoid(), voxel(), boolean(). WR_OBJ=<folder>
  also writes each mesh as an OBJ there (WR_OBJ_ONLY=1: only that) to look it over in Blender first.
Materials are set where they're used (the dressing's M_Rock / M_IceBlock / M_Obsidian, world-space projected, so the
meshes need no UVs to speak of).
"""
import math
import os
import random
import unreal

GS = unreal.GeometryScript_Primitives
DEF = unreal.GeometryScript_MeshDeformers
XF = unreal.GeometryScript_MeshTransforms
ASSET = unreal.GeometryScript_NewAssetUtils
ONLY = [n.strip() for n in os.environ.get("WR_MESHES", "").split(",") if n.strip()]
FOLDER = "/Game/Art/Gen/"


def want(name):
    return not ONLY or name in ONLY


def xf(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    """rot is (pitch, yaw, roll) as FRotator reads. (Python's unreal.Rotator takes roll, pitch, yaw: passed straight
    through, 11-22's first meshes had their basalt columns tipped 30 degrees and the obsidian blades spun end over end.)"""
    return unreal.Transform(unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]), unreal.Vector(*scale))


def opts():
    o = unreal.GeometryScriptPrimitiveOptions()
    o.set_editor_property("polygroup_mode", unreal.GeometryScriptPrimitivePolygroupMode.PER_FACE)
    return o


def noise(mesh, magnitude, frequency, seed):
    o = unreal.GeometryScriptPerlinNoiseOptions()
    layer = o.get_editor_property("base_layer")
    layer.set_editor_property("magnitude", magnitude)
    layer.set_editor_property("frequency", frequency)
    layer.set_editor_property("random_seed", seed)
    o.set_editor_property("base_layer", layer)
    DEF.apply_perlin_noise_to_mesh2(mesh, unreal.GeometryScriptMeshSelection(), o)


def dump_obj(mesh, name):
    """WR_OBJ=<folder>: also write the mesh as an OBJ there (to look it over in Blender before the game)."""
    folder = os.environ.get("WR_OBJ")
    if not folder:
        return
    Q, L = unreal.GeometryScript_MeshQueries, unreal.GeometryScript_List

    def pick(r, kind):
        return next(x for x in (r if isinstance(r, tuple) else (r,)) if isinstance(x, kind))
    vs = L.convert_vector_list_to_array(pick(Q.get_all_vertex_positions(mesh, False), unreal.GeometryScriptVectorList))
    ts = L.convert_triangle_list_to_array(pick(Q.get_all_triangle_indices(mesh, True), unreal.GeometryScriptTriangleList))
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, name + ".obj"), "w") as f:
        f.write("".join(f"v {v.x:.1f} {v.y:.1f} {v.z:.1f}\n" for v in vs))
        f.write("".join(f"f {t.x + 1} {t.z + 1} {t.y + 1}\n" for t in ts))


def save(mesh, name, nanite=False):
    dump_obj(mesh, name)
    if os.environ.get("WR_OBJ_ONLY"):
        unreal.log(f"[Wildreach] mesh {name}: obj only")
        return
    path = FOLDER + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    o = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    o.set_editor_property("enable_recompute_normals", True)
    o.set_editor_property("enable_recompute_tangents", True)
    o.set_editor_property("enable_collision", True)
    o.set_editor_property("collision_mode", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)   # the shape itself collides
    if nanite:
        o.set_editor_property("enable_nanite", True)
    sm, outcome = ASSET.create_new_static_mesh_asset_from_mesh(mesh, path, o)
    if sm:
        unreal.EditorAssetLibrary.save_loaded_asset(sm)
    unreal.log(f"[Wildreach] mesh {name}: {outcome} ({mesh.get_triangle_count() if hasattr(mesh, 'get_triangle_count') else '?'} tris)")


def basalt(name, seed):
    """A cluster of hexagonal columns: a honeycomb of them (axial hex grid), heights stepped like a broken stair."""
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    r = 34.0                                  # a column's circumradius (cm): about 0.6 m across the flats
    w = r * math.sqrt(3.0)                    # centre to centre across the flats
    cells = [(q, s) for q in range(-2, 3) for s in range(-2, 3) if abs(q + s) <= 2 and rnd.random() < 0.85]
    for q, s in cells:
        x = w * (q + s * 0.5)
        y = s * r * 1.5
        d = math.hypot(x, y)
        h = max(120.0, 560.0 - d * 2.2 - rnd.random() * 160.0)
        gap = rnd.uniform(0.9, 2.2)
        GS.append_cylinder(mesh, opts(), xf((x, y, -40), (0, 30, 0)), r - gap, h + 40, 6, 1, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)
        # a broken, tilted top: a short slab tipped a few degrees, now and then
        if rnd.random() < 0.45:
            GS.append_cylinder(mesh, opts(), xf((x, y, h - 2), (rnd.uniform(-7, 7), 30, rnd.uniform(-7, 7))), r - gap - 1.5, rnd.uniform(14, 30), 6, 1, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    noise(mesh, 2.5, 0.02, seed)
    save(mesh, name)


def blade(mesh, rnd, loc, height, width, lean, yaw):
    # A thin faceted blade: a five-sided point squashed flat, leaning out, a stub of the same glass at its foot.
    GS.append_cone(mesh, opts(), xf(loc, (lean, yaw, 0), (1.0, rnd.uniform(0.22, 0.32), 1.0)), width, rnd.uniform(1.5, 4.0), height, 5, 3, True, unreal.GeometryScriptPrimitiveOriginMode.BASE)


def obsidian(name, seed):
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    blade(mesh, rnd, (0, 0, -30), 420.0, 70.0, rnd.uniform(-6, 6), rnd.uniform(0, 360))
    for k in range(rnd.randint(2, 4)):
        a = rnd.uniform(0, 360)
        d = rnd.uniform(30, 70)
        blade(mesh, rnd, (math.cos(math.radians(a)) * d, math.sin(math.radians(a)) * d, -20), rnd.uniform(150, 320), rnd.uniform(35, 55), rnd.uniform(-22, 22), a)
    noise(mesh, 3.0, 0.03, seed)
    save(mesh, name)


def sulfur(name, seed):
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    # a double point: two four-sided points base to base, longer above (sulfur's bipyramids), 100 cm long all told
    GS.append_cone(mesh, opts(), xf((0, 0, 30), (0, 0, 0)), 22.0, 0.5, 70.0, 4, 1, False, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    GS.append_cone(mesh, opts(), xf((0, 0, 30), (180, 0, 0)), 22.0, 0.5, 30.0, 4, 1, False, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    XF.translate_mesh(mesh, unreal.Vector(0, 0, 0))
    save(mesh, name)


def serac(name, seed):
    """A tower of ice broken off a glacier's front, in one piece (11-22: stacked boxes, even with noise, read as stacked
    cubes, totems): a leaning column of five to seven sheared faces, narrowing, its top snapped off at a slant with a
    ragged crest, meltwater runnels down some faces; a lesser block fused to one side, broken chunks at its foot."""
    rnd = random.Random(seed)
    raw = Raw()
    sides = rnd.randint(5, 7)
    ax, ay = rnd.uniform(170, 230), rnd.uniform(110, 160)
    angs = sorted(2 * math.pi * k / sides + rnd.uniform(-0.3, 0.3) * 2 * math.pi / sides for k in range(sides))
    base = [(math.cos(a) * ax * rnd.uniform(0.85, 1.1), math.sin(a) * ay * rnd.uniform(0.85, 1.1)) for a in angs]
    H = rnd.uniform(650, 950)
    sl = (rnd.uniform(-0.7, 0.7), rnd.uniform(-0.7, 0.7))
    prism(raw, rnd, seed, base, -80.0, lambda x, y: H + sl[0] * x + sl[1] * y + 70.0 * fbm(x / 90, y / 90, 0, seed + 5),
          nz=12, nw=5, mag=26.0, profile=lambda t: 1.0 - 0.3 * t ** 1.3, lean=(rnd.uniform(-0.05, 0.05), rnd.uniform(0.06, 0.15)), flutes=22.0, cap_jag=60.0)
    # a lesser block fused to its side
    a = rnd.uniform(0, 2 * math.pi)
    c2 = (math.cos(a) * ax * 0.85, math.sin(a) * ay * 0.85)
    r2, h2 = rnd.uniform(90, 130), H * rnd.uniform(0.35, 0.6)
    b2 = [(c2[0] + math.cos(q) * r2 * rnd.uniform(0.8, 1.1), c2[1] + math.sin(q) * r2 * rnd.uniform(0.8, 1.1)) for q in sorted(rnd.uniform(0, 2 * math.pi) for _ in range(5))]
    prism(raw, rnd, seed + 11, b2, -70.0, lambda x, y: h2 + 0.5 * (x - c2[0]) + 40.0 * fbm(x / 70, y / 70, 1, seed), nz=7, nw=3, mag=16.0,
          profile=lambda t: 1.0 - 0.25 * t, lean=(rnd.uniform(-0.08, 0.08), rnd.uniform(-0.08, 0.08)), cap_jag=35.0)
    # chunks fallen at its foot
    for k in range(rnd.randint(2, 3)):
        a = rnd.uniform(0, 2 * math.pi)
        c3 = (math.cos(a) * (ax + 90), math.sin(a) * (ay + 90))
        r3 = rnd.uniform(45, 80)
        b3 = [(c3[0] + math.cos(q) * r3 * rnd.uniform(0.7, 1.1), c3[1] + math.sin(q) * r3 * rnd.uniform(0.7, 1.1)) for q in sorted(rnd.uniform(0, 2 * math.pi) for _ in range(rnd.randint(4, 6)))]
        h3 = r3 * rnd.uniform(0.7, 1.2)
        prism(raw, rnd, seed + 20 + k, b3, -30.0, lambda x, y, h3=h3, c3=c3: h3 + 0.6 * (y - c3[1]), nz=3, nw=2, mag=8.0, cap_jag=15.0)
    save(raw.mesh(), name)


def rubble(name, seed):
    """Broken basalt lying on the lava (11-22: the lava fields read as smooth sand): angular stones, scattered thicker and
    bigger toward the middle, each tipped its own way and half sunk, their faces split by noise."""
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    for k in range(42):
        r = 220.0 * math.sqrt(rnd.random()) * (0.55 + 0.45 * rnd.random())
        a = rnd.uniform(0, 2 * math.pi)
        big = 1.0 - r / 260.0
        sx = rnd.uniform(6, 22) + 26 * big * rnd.random()
        sy = sx * rnd.uniform(0.5, 1.0)
        sz = sx * rnd.uniform(0.35, 0.8)
        GS.append_box(mesh, opts(), xf((math.cos(a) * r, math.sin(a) * r, -sz * 0.45), (rnd.uniform(-35, 35), rnd.uniform(0, 360), rnd.uniform(-35, 35))), sx, sy, sz, 2, 2, 1, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    noise(mesh, 3.5, 0.06, seed)
    save(mesh, name, True)


def tent(name, seed, door):
    """A ridge tent's canvas (11-22: the camps' tents were a cube turned 45 degrees, a flat beige prism)."""
    rnd = random.Random(seed)
    L, HW, H = 280.0, 145.0, 150.0
    NX, NZ = 12, 7
    pts, tris = [], []

    def pt(p):
        pts.append(unreal.Vector(*p))
        return len(pts) - 1

    def face(a, b, c, out):
        # wound so the DynamicMesh's face normal points out: it is cross(c - a, b - a) in these terms (checked on the first
        # face: wound the other way the tent was inside out)
        pa, pb, pc = pts[a], pts[b], pts[c]
        u = (pb.x - pa.x, pb.y - pa.y, pb.z - pa.z)
        v = (pc.x - pa.x, pc.y - pa.y, pc.z - pa.z)
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        tris.append((a, c, b) if n[0] * out[0] + n[1] * out[1] + n[2] * out[2] >= 0 else (a, b, c))

    def sheet(grid, out, inset=0.6):
        """A grid of points (rows of columns) as quads facing out, and again inset, facing in."""
        for side, sign in ((0, 1.0), (1, -1.0)):
            ids = [[pt((p[0] - out[0] * inset * side, p[1] - out[1] * inset * side, p[2] - out[2] * inset * side)) for p in row] for row in grid]
            o = tuple(sign * c for c in out)
            for i in range(len(grid) - 1):
                for j in range(len(grid[0]) - 1):
                    face(ids[i][j], ids[i + 1][j], ids[i + 1][j + 1], o)
                    face(ids[i][j], ids[i + 1][j + 1], ids[i][j + 1], o)

    def tri(a, b, c, out, inset=0.6):
        for side, sign in ((0, 1.0), (1, -1.0)):
            ids = [pt((p[0] - out[0] * inset * side, p[1] - out[1] * inset * side, p[2] - out[2] * inset * side)) for p in (a, b, c)]
            face(ids[0], ids[1], ids[2], tuple(sign * c for c in out))

    ridge = lambda x: H - 7.0 * (1.0 - (2.0 * x / L) ** 2)
    ln = math.hypot(H, HW)
    for s in (-1.0, 1.0):
        ny, nz = s * H / ln, HW / ln
        grid = []
        for i in range(NX + 1):
            x = -L / 2 + L * i / NX
            row = []
            for j in range(NZ + 2):
                if j < NZ:
                    t = j / NZ
                    sag = 0.0 if j == 0 else 7.0 * math.sin(math.pi * t) * (0.45 + 0.55 * math.sin(math.pi * i / NX)) + rnd.uniform(-0.7, 0.7)
                    row.append((x, s * HW * t - ny * sag, ridge(x) * (1.0 - t) - nz * sag))
                elif j == NZ:
                    row.append((x, s * (HW + 4.0), 0.0))          # the hem, flared
                else:
                    row.append((x, s * (HW + 9.0), -6.0))         # tucked into the ground
            grid.append(row)
        sheet(grid, (0.0, ny, nz))
    # the back gable, closed
    xb = L / 2
    tri((xb, 0.0, H), (xb, -(HW + 4.0), 0.0), (xb, HW + 4.0, 0.0), (1.0, 0.0, 0.0))
    sheet([[(xb, -(HW + 4.0), 0.0), (xb, HW + 4.0, 0.0)], [(xb, -(HW + 9.0), -6.0), (xb, HW + 9.0, -6.0)]], (1.0, 0.0, 0.0))
    # the front gable: two flaps either side of the doorway (door = half its width at the ground; 0 = closed)
    xf_ = -L / 2
    for s in (-1.0, 1.0):
        tri((xf_, 0.0, H), (xf_, s * (HW + 4.0), 0.0), (xf_, s * door, 0.0), (-1.0, 0.0, 0.0))
        sheet([[(xf_, s * door, 0.0), (xf_, s * (HW + 4.0), 0.0)], [(xf_, s * door, -6.0), (xf_, s * (HW + 9.0), -6.0)]], (-1.0, 0.0, 0.0))
    # a groundsheet inside (what you see through the doorway)
    sheet([[(-L / 2 + 4, -HW + 6, 1.5), (-L / 2 + 4, HW - 6, 1.5)], [(L / 2 - 4, -HW + 6, 1.5), (L / 2 - 4, HW - 6, 1.5)]], (0.0, 0.0, 1.0), 0.0)
    mesh = unreal.DynamicMesh()
    unreal.GeometryScript_MeshEdits.add_vertices_to_mesh(mesh, unreal.GeometryScript_List.convert_array_to_vector_list(pts))
    unreal.GeometryScript_MeshEdits.add_triangles_to_mesh(mesh, unreal.GeometryScript_List.convert_array_to_triangle_list([unreal.IntVector(*t) for t in tris]), 0)
    fn = unreal.GeometryScript_MeshQueries.get_triangle_face_normal(mesh, 0)
    unreal.log(f"[Wildreach] tent first face normal {fn} (out: -Y and up)")
    save(mesh, name)


class Raw:
    """A mesh from points and triangles, each triangle wound so its face points along the outward hint given (the
    DynamicMesh's front is cross(c - a, b - a): see tent())."""

    def __init__(self):
        self.pts, self.tris = [], []

    def pt(self, p):
        self.pts.append(p)
        return len(self.pts) - 1

    def face(self, a, b, c, out):
        pa, pb, pc = self.pts[a], self.pts[b], self.pts[c]
        u = (pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2])
        v = (pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2])
        n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
        self.tris.append((a, c, b) if n[0] * out[0] + n[1] * out[1] + n[2] * out[2] >= 0 else (a, b, c))

    def grid(self, ids, out_of, wrap=True):
        """Quads over a grid of point ids (rows x columns, the columns wrapping round unless wrap is False); out_of(p)
        gives each face's outward hint from its centre."""
        rows, cols = len(ids), len(ids[0])
        for i in range(rows - 1):
            for j in range(cols if wrap else cols - 1):
                a, b, c, d = ids[i][j], ids[i][(j + 1) % cols], ids[i + 1][(j + 1) % cols], ids[i + 1][j]
                ctr = [sum(self.pts[k][m] for k in (a, b, c, d)) / 4.0 for m in range(3)]
                o = out_of(ctr)
                self.face(a, b, c, o)
                self.face(a, c, d, o)

    def fan(self, ring, centre, out):
        c = self.pt(centre)
        for j in range(len(ring)):
            self.face(c, ring[j], ring[(j + 1) % len(ring)], out)

    def mesh(self):
        m = unreal.DynamicMesh()
        unreal.GeometryScript_MeshEdits.add_vertices_to_mesh(m, unreal.GeometryScript_List.convert_array_to_vector_list([unreal.Vector(*p) for p in self.pts]))
        unreal.GeometryScript_MeshEdits.add_triangles_to_mesh(m, unreal.GeometryScript_List.convert_array_to_triangle_list([unreal.IntVector(*t) for t in self.tris]), 0)
        return m


def fbm(x, y, z, seed, octaves=3):
    """Smooth value noise in -1..1 (a hashed lattice, eased), a few octaves."""
    def h(i, j, k):
        n = (i * 73856093) ^ (j * 19349663) ^ (k * 83492791) ^ (seed * 2654435761)
        n = (n ^ (n >> 13)) * 1274126177
        return ((n ^ (n >> 16)) & 0xFFFF) / 32767.5 - 1.0

    def one(x, y, z):
        i, j, k = math.floor(x), math.floor(y), math.floor(z)
        fx, fy, fz = x - i, y - j, z - k
        sx, sy, sz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
        def l(a, b, t):
            return a + (b - a) * t
        return l(l(l(h(i, j, k), h(i + 1, j, k), sx), l(h(i, j + 1, k), h(i + 1, j + 1, k), sx), sy),
                 l(l(h(i, j, k + 1), h(i + 1, j, k + 1), sx), l(h(i, j + 1, k + 1), h(i + 1, j + 1, k + 1), sx), sy), sz)
    total, amp, f, norm = 0.0, 1.0, 1.0, 0.0
    for _ in range(octaves):
        total += amp * one(x * f, y * f, z * f)
        norm += amp
        amp *= 0.5
        f *= 2.03
    return total / norm


def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3 - 2 * t)


def ice_arch(name, seed):
    """A natural ice bridge (Grimjaw's den): an irregular, blocky section swept along an arch 72 m across and 13 m high,
    thicker and wider at its feet (which run on 6 m into the ground), its faces split by noise, icicles under it.
    Along Y, its feet at Y = +-3600, z = 0 there; its top walkable (it collides as its own shape)."""
    rnd = random.Random(seed)
    raw = Raw()
    NS, NR = 64, 22
    SPAN, RISE, OVER = 3600.0, 1300.0, 1.09

    def centre(u):
        a = abs(u)
        z = RISE * (1.0 - min(a, 1.0) ** 2) ** 0.8 if a <= 1.0 else -(a - 1.0) / (OVER - 1.0) * 650.0
        return (0.0, SPAN * u, z)

    rings = []
    for i in range(NS + 1):
        u = -OVER + 2 * OVER * i / NS
        c = centre(u)
        c1, c0 = centre(min(OVER, u + 0.01)), centre(max(-OVER, u - 0.01))
        t = (c1[1] - c0[1], c1[2] - c0[2])
        tl = math.hypot(*t)
        ny, nz = -t[1] / tl, t[0] / tl                      # the section's up: across the curve in the YZ plane
        if nz < 0:
            ny, nz = -ny, -nz
        foot = smooth(0.55, 1.0, abs(u))
        hw = 300.0 + 180.0 * foot + 30.0 * fbm(u * 3, 0, 0, seed + 1)
        hh = 170.0 + 140.0 * foot
        ring = []
        for j in range(NR):
            th = 2 * math.pi * j / NR
            cx, cz = math.cos(th), math.sin(th)
            # a squarish section (superellipse), the underside rougher
            e = 2.6
            sx = math.copysign(abs(cx) ** (2 / e), cx)
            sz = math.copysign(abs(cz) ** (2 / e), cz)
            px, pz = sx * hw, sz * hh
            p = (c[0] + px, c[1] + ny * pz, c[2] + nz * pz)
            nrm = (cx, ny * cz, nz * cz)
            d = 85.0 * fbm(p[0] / 420, p[1] / 420, p[2] / 420, seed + 2) + 28.0 * fbm(p[0] / 130, p[1] / 130, p[2] / 130, seed + 3)
            if cz < -0.3:
                d += 40.0 * abs(fbm(p[0] / 200, p[1] / 200, 0, seed + 4))
            ring.append(raw.pt((p[0] + nrm[0] * d, p[1] + nrm[1] * d, p[2] + nrm[2] * d)))
        rings.append((ring, c))
    ids = [r for r, _ in rings]
    cs = [c for _, c in rings]

    def out_of(p):
        # away from the nearest centre point on the curve
        best = min(cs, key=lambda c: (c[0] - p[0]) ** 2 + (c[1] - p[1]) ** 2 + (c[2] - p[2]) ** 2)
        return (p[0] - best[0], p[1] - best[1], p[2] - best[2])
    raw.grid(ids, out_of)
    raw.fan(ids[0], cs[0], (0.0, -1.0, 0.0))
    raw.fan(ids[-1], cs[-1], (0.0, 1.0, 0.0))
    mesh = raw.mesh()
    # icicles hanging from its underside
    for k in range(26):
        u = rnd.uniform(-0.75, 0.75)
        c = centre(u)
        hh = 170.0 + 140.0 * smooth(0.55, 1.0, abs(u))
        x = rnd.uniform(-220, 220)
        ln = rnd.uniform(70, 260)
        GS.append_cone(mesh, opts(), xf((x, c[1], c[2] - hh * 0.8), (180, 0, 0)), rnd.uniform(18, 45), 1.0, ln, 6, 2, False, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    save(mesh, name)


def pingo(name, seed):
    """An ice-cored mound (the Fox Circle Pingo): a dome 26 m across and 10.5 m high, a skirt running on into the ground,
    a five-rayed crack in its crown and one flank broken open (steep faces: the ice material shows ice there, snow on the
    rest). Its origin at the centre of its foot."""
    rnd = random.Random(seed)
    raw = Raw()
    R, H, NA = 1300.0, 1050.0, 64
    radii = [18.0 + (R * 1.12 - 18.0) * (i / 30.0) ** 1.15 for i in range(31)]
    rays = [rnd.uniform(0, 360) + 72 * k for k in range(5)]
    ids = []
    for r in radii:
        ring = []
        for j in range(NA):
            az = 360.0 * j / NA
            a = math.radians(az)
            if r <= R:
                z = H * (1.0 - (r / R) ** 2) ** 1.15
            else:
                z = -(r - R) / (R * 0.12) * 700.0
            rr = r
            # the crown's crack: grooves along five rays
            if r < 560:
                for ray in rays:
                    d = abs((az - ray + 180) % 360 - 180) * math.pi / 180 * r
                    z -= 120.0 * (1.0 - r / 560.0) ** 0.6 * math.exp(-(d / 38.0) ** 2)
            # the flank broken open toward 210 degrees: pulled in, steep-walled
            da = abs((az - 210 + 180) % 360 - 180)
            f = (1.0 - smooth(22.0, 48.0, da + 9.0 * fbm(r / 300.0, 0, 0, seed + 9))) * smooth(380.0, 820.0, r) * (1.0 - smooth(R * 1.0, R * 1.1, r))
            rr = r - 220.0 * f
            z -= 120.0 * f
            p = (math.cos(a) * rr, math.sin(a) * rr, z)
            n = 70.0 * fbm(p[0] / 520, p[1] / 520, p[2] / 520, seed + 5) + 22.0 * fbm(p[0] / 140, p[1] / 140, p[2] / 140, seed + 6)
            ring.append(raw.pt((p[0] + math.cos(a) * n * 0.6, p[1] + math.sin(a) * n * 0.6, p[2] + n * 0.8 * (1.0 if r < R else 0.3))))
        ids.append(ring)
    raw.grid(ids, lambda p: (p[0], p[1], p[2] + 2500.0))
    raw.fan(ids[0], (0.0, 0.0, H - 60.0), (0.0, 0.0, 1.0))
    save(raw.mesh(), name)


def ice_block(name, seed):
    """One block of glacier ice 4 m each way (the Crevasse Maze's rows, bridges): finely divided so its faces split and
    slump, its top broken at a slant, a chip off one corner. Origin at the centre of its foot; it runs 0.6 m into the
    ground. (Squashed seracs read as piles of cubes at this size.)"""
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    GS.append_box(mesh, opts(), xf((0, 0, -60)), 400.0, 400.0, 460.0, 10, 10, 10, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    # the top broken at a slant, then split
    tilt = (rnd.uniform(-0.18, 0.18), rnd.uniform(-0.18, 0.18))
    n = unreal.GeometryScript_MeshQueries.get_vertex_count(mesh)
    for v in range(n):
        p, ok = unreal.GeometryScript_MeshQueries.get_vertex_position(mesh, v)
        if p.z > 0:
            k = (p.z + 60.0) / 460.0
            dz = (p.x * tilt[0] + p.y * tilt[1]) * k
            # a chip off the corner toward +x +y
            chip = max(0.0, (p.x + p.y) / 400.0 - 0.55) * 160.0 * k
            unreal.GeometryScript_MeshEdits.set_vertex_position(mesh, v, unreal.Vector(p.x, p.y, p.z + dz - chip), True)
    noise(mesh, 36.0, 0.007, seed)
    noise(mesh, 10.0, 0.03, seed + 3)
    save(mesh, name)


def hornito(name, seed):
    """A lava chimney (hornito) of welded spatter: a lumpy tapering stack 20 m tall and 6 m across its foot, clots in rough
    bands, an open throat at the top going down into the dark. Origin at the centre of its foot; it runs 1.5 m on into
    the ground. Scaled per use (the Bellows Stacks, the Volcanic's small hornitos)."""
    rnd = random.Random(seed)
    raw = Raw()
    R, H, NA, NZ = 300.0, 2000.0, 28, 40
    ids = []
    for i in range(NZ + 1):
        t = i / NZ
        z = -150.0 + (H + 150.0) * t
        base_r = R * (1.0 - 0.6 * max(0.0, z / H) ** 0.8)
        ring = []
        for j in range(NA):
            a = 2 * math.pi * j / NA
            band = 0.08 * math.sin(z / H * 2 * math.pi * 7.0 + fbm(math.cos(a), math.sin(a), 0, seed) * 3.0)
            lump = 0.16 * fbm(math.cos(a) * 2.2, math.sin(a) * 2.2, z / 260.0, seed + 1) + 0.07 * fbm(math.cos(a) * 6, math.sin(a) * 6, z / 90.0, seed + 2)
            r = base_r * (1.0 + band + lump)
            ring.append(raw.pt((math.cos(a) * r, math.sin(a) * r, z)))
        ids.append(ring)
    # the lip, then the throat down inside
    lip_r = R * 0.4 * 0.62
    lip = [raw.pt((math.cos(2 * math.pi * j / NA) * lip_r, math.sin(2 * math.pi * j / NA) * lip_r, H + 30.0 + 25.0 * fbm(j * 0.4, 0, 0, seed + 3))) for j in range(NA)]
    throat = [raw.pt((math.cos(2 * math.pi * j / NA) * lip_r * 0.8, math.sin(2 * math.pi * j / NA) * lip_r * 0.8, H - 260.0)) for j in range(NA)]
    raw.grid(ids + [lip], lambda p: (p[0], p[1], 0.0) if p[2] < H - 1 else (p[0], p[1], 300.0))
    raw.grid([lip, throat], lambda p: (-p[0], -p[1], 0.0))
    raw.fan(throat, (0.0, 0.0, H - 300.0), (0.0, 0.0, 1.0))
    raw.fan(ids[0], (0.0, 0.0, -160.0), (0.0, 0.0, -1.0))
    save(raw.mesh(), name)


# ------------------------------------------------------------------------- the last places (11-22, the look's round 4)
def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_mul(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def v_norm(a):
    ln = math.sqrt(v_dot(a, a)) or 1.0
    return (a[0] / ln, a[1] / ln, a[2] / ln)


def spline(ctrl, n):
    """n points (ends included) on a Catmull-Rom curve through the control points."""
    P = [ctrl[0]] + list(ctrl) + [ctrl[-1]]
    segs = len(ctrl) - 1
    out = []
    for i in range(n):
        u = i / (n - 1) * segs
        k = min(int(u), segs - 1)
        t = u - k
        p0, p1, p2, p3 = P[k], P[k + 1], P[k + 2], P[k + 3]
        out.append(tuple(0.5 * (2 * p1[m] + (p2[m] - p0[m]) * t + (2 * p0[m] - 5 * p1[m] + 4 * p2[m] - p3[m]) * t * t
                                + (3 * p1[m] - p0[m] - 3 * p2[m] + p3[m]) * t * t * t) for m in range(3)))
    return out


def tube(raw, path, radius, na=16, side=None, disp=None, caps=True, jag=None):
    """A ring swept along a path of points. radius(t) gives the ring's half-sizes (a, b), t 0..1 along the path; the a
    axis lies along side(p, tangent) where given (made square to the path), else a frame carried along the path. disp(p,
    t, th) pushes each point out (noise). jag = (rnd, cm): the far end broken off, its ring pushed along the path unevenly."""
    n = len(path)
    T = [v_norm(v_sub(path[min(n - 1, i + 1)], path[max(0, i - 1)])) for i in range(n)]
    rings, prev = [], None
    for i in range(n):
        t = i / (n - 1)
        if side is not None:
            ref = side(path[i], T[i])
        elif prev is not None:
            ref = prev
        else:
            ref = (0.0, 0.0, 1.0) if abs(T[0][2]) < 0.9 else (1.0, 0.0, 0.0)
        A = v_norm(v_sub(ref, v_mul(T[i], v_dot(ref, T[i]))))
        prev = A
        B = v_cross(T[i], A)
        ra, rb = radius(t)
        ring = []
        for j in range(na):
            th = 2 * math.pi * j / na
            off = v_add(v_mul(A, math.cos(th) * ra), v_mul(B, math.sin(th) * rb))
            p = v_add(path[i], off)
            if disp:
                p = v_add(p, v_mul(v_norm(off), disp(p, t, th)))
            if jag and i == n - 1:
                p = v_add(p, v_mul(T[i], jag[0].uniform(-jag[1], jag[1])))
            ring.append(raw.pt(p))
        rings.append(ring)

    def out_of(c):
        k = min(range(n), key=lambda q: (path[q][0] - c[0]) ** 2 + (path[q][1] - c[1]) ** 2 + (path[q][2] - c[2]) ** 2)
        return v_sub(c, path[k])
    raw.grid(rings, out_of)
    if caps:
        raw.fan(rings[0], path[0], v_mul(T[0], -1.0))
        raw.fan(rings[-1], path[-1], T[-1])
    return rings


def ellipsoid(raw, c, r, axes=((1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)), nu=12, nv=20, disp=None):
    """A closed ellipsoid at c, half-sizes r along its three axes (unit vectors)."""
    rings = []
    for i in range(1, nu):
        lat = -math.pi / 2 + math.pi * i / nu
        ring = []
        for j in range(nv):
            lon = 2 * math.pi * j / nv
            l3 = (math.cos(lat) * math.cos(lon) * r[0], math.cos(lat) * math.sin(lon) * r[1], math.sin(lat) * r[2])
            off = v_add(v_add(v_mul(axes[0], l3[0]), v_mul(axes[1], l3[1])), v_mul(axes[2], l3[2]))
            p = v_add(c, off)
            if disp:
                p = v_add(p, v_mul(v_norm(off), disp(p)))
            ring.append(raw.pt(p))
        rings.append(ring)
    raw.grid(rings, lambda q: v_sub(q, c))
    raw.fan(rings[0], v_sub(c, v_mul(axes[2], r[2])), v_mul(axes[2], -1.0))
    raw.fan(rings[-1], v_add(c, v_mul(axes[2], r[2])), axes[2])


def prism(raw, rnd, seed, base, z0, top, nz=10, nw=4, mag=18.0, profile=None, lean=(0.0, 0.0), flutes=0.0, cap_jag=30.0):
    """A faceted column standing on the corners `base` [(x, y)] (going round): each side its own strip of points, so its
    edges stay sharp, pushed out along its face by noise (none at the corners, where the strips meet); narrowing by
    profile(t) up it; leaning (cm out per cm up); each corner's top at top(x, y) (a slanted, broken top); a cap stepping
    in to the middle with a ragged crest. flutes: runnels (cm deep) down some faces."""
    n = len(base)
    cx = sum(p[0] for p in base) / n
    cy = sum(p[1] for p in base) / n
    edge, bottom = [], []
    for k in range(n):
        a, b = base[k], base[(k + 1) % n]
        ex, ey = b[0] - a[0], b[1] - a[1]
        nx, ny = ey, -ex
        if nx * ((a[0] + b[0]) / 2 - cx) + ny * ((a[1] + b[1]) / 2 - cy) < 0:
            nx, ny = -nx, -ny
        ln = math.hypot(nx, ny) or 1.0
        nx, ny = nx / ln, ny / ln
        g = rnd.uniform(0.2, 0.8) if flutes and rnd.random() < 0.65 else None
        ids = []
        for i in range(nz + 1):
            t = i / nz
            s = profile(t) if profile else 1.0
            row = []
            for j in range(nw + 1):
                u = j / nw
                x0, y0 = a[0] + ex * u, a[1] + ey * u
                z = z0 + (top(x0, y0) - z0) * t
                x1, y1 = cx + (x0 - cx) * s, cy + (y0 - cy) * s
                d = mag * math.sin(math.pi * u) ** 0.6 * (fbm(x1 / 260, y1 / 260, z / 260, seed) + 0.35 * fbm(x1 / 70, y1 / 70, z / 70, seed + 1))
                if g is not None:
                    d -= flutes * math.exp(-((u - g) / 0.07) ** 2) * (0.4 + 0.6 * t)
                row.append(raw.pt((x1 + nx * d + lean[0] * (z - z0), y1 + ny * d + lean[1] * (z - z0), z)))
            ids.append(row)
        raw.grid(ids, lambda c, nx=nx, ny=ny: (nx, ny, 0.0), wrap=False)
        for j in range(nw):
            edge.append(raw.pts[ids[nz][j]])
            bottom.append(raw.pts[ids[0][j]])
    zc = top(cx, cy)
    centre = (cx + lean[0] * (zc - z0), cy + lean[1] * (zc - z0), zc + cap_jag * 0.5)
    rings = []
    for f in (0.0, 0.35, 0.7):
        ring = []
        for p in edge:
            q = (p[0] + (centre[0] - p[0]) * f, p[1] + (centre[1] - p[1]) * f, p[2] + (centre[2] - p[2]) * f)
            if f > 0:
                q = (q[0], q[1], q[2] + cap_jag * fbm(q[0] / 60, q[1] / 60, 0, seed + 3))
            ring.append(raw.pt(q))
        rings.append(ring)
    raw.grid(rings, lambda c: (0.0, 0.0, 1.0))
    raw.fan(rings[-1], centre, (0.0, 0.0, 1.0))
    raw.fan([raw.pt(p) for p in bottom], (cx, cy, z0), (0.0, 0.0, -1.0))


VOX = getattr(unreal, "GeometryScript_MeshVoxelProcessing", None)


def enum_value(fragment, member):
    for n in dir(unreal):
        if fragment in n:
            e = getattr(unreal, n, None)
            if e is not None and hasattr(e, member):
                return getattr(e, member)
    return None


def grid_params(g, cell):
    g.set_editor_property("size_method", enum_value("GridSizingMethod", "GRID_CELL_SIZE"))
    g.set_editor_property("grid_cell_size", cell)
    return g


def voxel(mesh, cell, close=0.0, smooth_n=0, tris=0):
    """Overlapping closed parts made one surface (voxel solidify, cell cm), the inside corners where they meet filled
    round (a morphological close of `close` cm: the parts grow together as bone does), relaxed, thinned to `tris`."""
    o = unreal.GeometryScriptSolidifyOptions()
    o.set_editor_property("grid_parameters", grid_params(o.get_editor_property("grid_parameters"), cell))
    VOX.apply_mesh_solidify(mesh, o)
    if close > 0:
        try:
            m = unreal.GeometryScriptMorphologyOptions()
            m.set_editor_property("operation", enum_value("MorphologicalOpType", "CLOSE"))
            m.set_editor_property("distance", close)
            m.set_editor_property("sdf_grid_parameters", grid_params(m.get_editor_property("sdf_grid_parameters"), cell))
            VOX.apply_mesh_morphology(mesh, m)
        except Exception as e:
            unreal.log_warning(f"[Wildreach] morphology close failed: {e}")
    if smooth_n:
        s = unreal.GeometryScriptIterativeMeshSmoothingOptions()
        s.set_editor_property("num_iterations", smooth_n)
        s.set_editor_property("alpha", 0.3)
        DEF.apply_iterative_smoothing_to_mesh(mesh, unreal.GeometryScriptMeshSelection(), s)
    if tris:
        unreal.GeometryScript_MeshSimplification.apply_simplify_to_triangle_count(mesh, tris, unreal.GeometryScriptSimplifyMeshOptions())


def boolean(mesh, tool, op):
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(mesh, unreal.Transform(), tool, unreal.Transform(), op, unreal.GeometryScriptMeshBooleanOptions())


def append(mesh, raw):
    unreal.GeometryScript_MeshEdits.append_mesh(mesh, raw.mesh(), unreal.Transform())


def tooth(raw, p, w, h, up):
    """A blunt tooth from p, h long, pointing down (up = -1) or up (1)."""
    tube(raw, [(p[0], p[1], p[2] - 20 * up), (p[0], p[1], p[2] + h * 0.5 * up), (p[0], p[1], p[2] + h * up)],
         lambda t: (w * (1 - 0.5 * t * t), w * 0.8 * (1 - 0.5 * t * t)), na=8)


def teeth_row(ctrl, up, seed, dz):
    """Fifteen teeth along a jaw's row, set close (they touch), broad molars at the back and narrower teeth in front,
    worn to uneven lengths, one lost."""
    rnd = random.Random(seed)
    raw = Raw()
    lost = rnd.choice((2, 3, 11, 12))
    for k, p in enumerate(spline(ctrl, 15)):
        if k == lost:
            continue
        f = abs(k - 7) / 7.0
        tooth(raw, (p[0], p[1], p[2] + dz), 21 + 9 * f, (44 - 10 * f) * rnd.uniform(0.75, 1.05), up)
    return raw


def giant_skull(name, seed):
    """The Fallen Smith's skull, a forge giant's, 5.6 m long: braincase, brow ridge, cheekbones and arches, the upper
    jaw and the ridge its teeth sit in, made one bone (voxel-merged, filled round where the parts meet); the eye
    sockets and the nose's opening cut deep into it; the teeth. Face toward +X, top up, origin at its middle."""
    raw = Raw()
    E = lambda c, r: ellipsoid(raw, c, r, nu=14, nv=22)
    E((-50, 0, 70), (235, 200, 185))      # braincase
    E((110, 0, 70), (150, 172, 150))      # forehead
    E((-140, 0, -40), (120, 150, 110))    # the base at the back
    E((200, 0, -105), (115, 118, 85))     # the upper jaw
    for s in (-1, 1):
        E((205, s * 132, -22), (60, 50, 55))          # cheekbones
        E((-20, s * 150, -120), (45, 40, 55))         # behind the ears
        tube(raw, spline([(215, s * 150, -10), (130, s * 192, -35), (30, s * 185, -30)], 10), lambda t: (26, 20), na=10)
    tube(raw, spline([(165, -178, 70), (215, -92, 98), (234, 0, 92), (215, 92, 98), (165, 178, 70)], 16), lambda t: (36, 30), na=12)
    tube(raw, spline([(232, 0, 80), (262, 0, 0), (284, 0, -48)], 8), lambda t: (26, 20), na=10)
    row = [(70, -128, -158), (170, -114, -174), (250, -56, -178), (270, 0, -178), (250, 56, -178), (170, 114, -174), (70, 128, -158)]
    tube(raw, spline(row, 24), lambda t: (34, 28), na=12)
    mesh = raw.mesh()
    voxel(mesh, 7.0, close=24.0, smooth_n=3)
    cut = Raw()
    for s in (-1, 1):
        a = math.radians(14 * s)
        ellipsoid(cut, (262, s * 88, 18), (118, 66, 60), axes=((math.cos(a), math.sin(a), 0.0), (-math.sin(a), math.cos(a), 0.0), (0.0, 0.0, 1.0)))
    ellipsoid(cut, (300, 0, -60), (70, 32, 50))
    ellipsoid(cut, (300, 0, -92), (62, 44, 30))
    tool = cut.mesh()
    voxel(tool, 6.0)   # the cutters as one closed surface (two overlapping left a disc in the nose)
    boolean(mesh, tool, unreal.GeometryScriptBooleanOperation.SUBTRACT)
    append(mesh, teeth_row(row, -1, seed, -18.0))
    noise(mesh, 4.0, 0.02, seed)
    noise(mesh, 1.5, 0.08, seed + 1)
    save(mesh, name)


def giant_jaw(name, seed):
    """The skull's lower jaw: one U of bone, its sides rising behind into the hinges, the chin at +X, its teeth up.
    Origin as the skull's (the hinges at about (-10, +-168, 115) under it)."""
    raw = Raw()
    radial = lambda p: v_norm((max(0.0, p[0] - 60.0), p[1], 0.0))
    side = lambda p, T: v_cross(T, radial(p))
    path = spline([(-10, -168, 110), (-24, -172, 10), (-18, -166, -70), (70, -142, -96), (170, -106, -102), (248, -52, -100), (272, 0, -98),
                   (248, 52, -100), (170, 106, -102), (70, 142, -96), (-18, 166, -70), (-24, 172, 10), (-10, 168, 110)], 64)
    # the sides rising to the hinges are broad plates (the ramus), the body a tall narrow bar
    tube(raw, path, lambda t: (40.0 + 22.0 * (1.0 - smooth(0.1, 0.2, min(t, 1.0 - t))), 17.0), na=14, side=side)
    for s in (-1, 1):
        ellipsoid(raw, (-14, s * 168, 116), (32, 44, 22))
        tube(raw, spline([(10, s * 165, 30), (45, s * 160, 95), (62, s * 156, 124)], 8), lambda t: (30 - 14 * t, 13), na=10, side=side)
    mesh = raw.mesh()
    voxel(mesh, 6.0, close=16.0, smooth_n=2)
    append(mesh, teeth_row([(60, -140, -62), (170, -104, -62), (248, -50, -62), (272, 0, -62), (248, 50, -62), (170, 104, -62), (60, 140, -62)], 1, seed, 0.0))
    noise(mesh, 3.0, 0.03, seed)
    save(mesh, name)


def giant_vertebra(name, seed):
    """A vertebra of the giant lying on its back: the drum (centrum) on top along X, the arch round the spinal canal
    under it, the spine pointing down into the clinker, the wings (transverse processes) out to either side, the joint
    knobs fore and aft. About 1.6 m long, 4.3 m across the wings. Origin at the drum's middle."""
    raw = Raw()
    tube(raw, [(-80 + 20 * i, 0, 0) for i in range(9)], lambda t: (82 - 12 * math.sin(math.pi * t),) * 2, na=24)
    tube(raw, spline([(0, -58, -40), (0, -66, -110), (0, -30, -160), (0, 0, -168), (0, 30, -160), (0, 66, -110), (0, 58, -40)], 16), lambda t: (26, 20), na=12)
    tube(raw, spline([(0, 0, -160), (-25, 0, -260), (-45, 0, -340)], 8), lambda t: (34 - 14 * t, 14 - 4 * t), na=12, side=lambda p, T: (1.0, 0.0, 0.0))
    for s in (-1, 1):
        tube(raw, spline([(0, s * 50, -105), (-5, s * 140, -100), (-15, s * 215, -80)], 8), lambda t: (24 - 8 * t, 18 - 6 * t), na=12)
        for fx in (-1, 1):
            ellipsoid(raw, (fx * 62, s * 50, -72), (30, 22, 26), nu=8, nv=12)
    mesh = raw.mesh()
    voxel(mesh, 6.0, close=10.0, smooth_n=2)
    noise(mesh, 2.5, 0.04, seed)
    save(mesh, name)


RIB = [(0, 70, 0), (-15, 300, 15), (-35, 520, 160), (-55, 625, 420), (-75, 590, 690), (-90, 440, 900), (-100, 250, 1000)]


def giant_rib(name, seed, t0, t1, lying=False):
    """A rib of the giant lying on its back (the +Y side): it leaves the spine (origin) outward along the ground, curves
    up round the body's side and in over it, its tip 10 m up and 2.5 m off the middle; flat (wide along the body), a
    knob where it meets the spine. t0..t1: the part of it (a stub snapped off, the piece that fell: jagged where it
    broke). lying: the piece laid flat on its side (origin under its middle)."""
    rnd = random.Random(seed)
    raw = Raw()
    full = spline(RIB, 49)
    path = full[int(round(t0 * 48)): int(round(t1 * 48)) + 1]
    # the broken end last (tube jags its far end): the fallen piece runs from its tip back to where it snapped
    flip = t0 > 0.0
    if flip:
        path = path[::-1]
    at = (lambda t: t1 - (t1 - t0) * t) if flip else (lambda t: t0 + (t1 - t0) * t)
    tube(raw, path, lambda t: (62 - 26 * at(t), 26 - 9 * at(t)), na=14,
         side=lambda p, T: (1.0, 0.0, 0.0), disp=lambda p, t, th: 4.0 * fbm(p[0] / 60, p[1] / 60, p[2] / 60, seed),
         jag=(rnd, 28.0) if (t1 < 1.0 or flip) else None)
    if t0 == 0.0:
        ellipsoid(raw, (0, 60, 0), (60, 45, 50), nu=10, nv=16)
    if lying:
        # turned onto its side: the curve's plane (YZ) laid flat, its middle over the origin, its lowest point at -10
        pts = [(p[2], p[1], -p[0]) for p in raw.pts]
        mx = (min(p[0] for p in pts) + max(p[0] for p in pts)) / 2
        my = (min(p[1] for p in pts) + max(p[1] for p in pts)) / 2
        mz = min(p[2] for p in pts)
        raw.pts = [(p[0] - mx, p[1] - my, p[2] - mz - 10.0) for p in pts]
    save(raw.mesh(), name)


def giant_long_bone(name, seed):
    """A thigh bone of the giant, 15 m along X from the hip end (origin): a shaft narrowing to its middle, the ball on its
    neck and the knob below it at the hip end, two rounded knuckles at the knee end. (Scaled for the shin and the arm.)"""
    raw = Raw()
    L = 1500.0
    tube(raw, [(L * i / 30, 0, 0) for i in range(31)], lambda t: ((52 + 30 * abs(2 * t - 1) ** 3), (52 + 30 * abs(2 * t - 1) ** 3) * 0.92), na=20,
         disp=lambda p, t, th: 3.0 * fbm(p[0] / 80, p[1] / 80, p[2] / 80, seed))
    tube(raw, spline([(60, 0, 20), (10, 0, 90), (-30, 0, 140)], 8), lambda t: (48, 44), na=16)
    ellipsoid(raw, (-40, 0, 150), (75, 72, 72))
    ellipsoid(raw, (40, 0, -60), (70, 55, 60))
    for s in (-1, 1):
        ellipsoid(raw, (L - 20, s * 48, -10), (85, 58, 78))
    mesh = raw.mesh()
    voxel(mesh, 8.0, close=18.0, smooth_n=3)
    noise(mesh, 3.0, 0.02, seed)
    save(mesh, name)


def giant_pelvis(name, seed):
    """The giant's hips, lying face up: the sacrum along the ground toward the spine (+X), the two hip blades rising up
    and out, the sockets at (-90, +-260) where the legs go off west (-X), the pubic arch meeting in front."""
    raw = Raw()
    ellipsoid(raw, (40, 0, 20), (170, 110, 55))                        # sacrum
    for s in (-1, 1):
        a = math.radians(40)
        ax1 = (0.0, s * math.cos(a), math.sin(a))
        ax2 = v_norm(v_cross((1.0, 0.0, 0.0), ax1))
        ellipsoid(raw, (40, s * 190, 140), (230, 170, 30), axes=((1.0, 0.0, 0.0), ax1, ax2), nu=12, nv=24)   # the blade
        tube(raw, spline([(150, s * 80, 110), (60, s * 230, 260), (-60, s * 300, 210)], 14), lambda t: (30, 26), na=12)   # its crest
        ellipsoid(raw, (60, s * 90, 60), (110, 70, 60))                # the joint with the sacrum
        ellipsoid(raw, (-90, s * 230, 30), (80, 70, 70))               # the hip socket
        tube(raw, spline([(0, s * 200, 80), (-50, s * 220, 60), (-90, s * 230, 40)], 8), lambda t: (52, 46), na=14)
        tube(raw, spline([(-90, s * 220, 50), (-170, s * 120, 130), (-205, 0, 150)], 12), lambda t: (34, 28), na=12)    # pubis
        tube(raw, spline([(-100, s * 240, 0), (-200, s * 180, 20), (-215, s * 115, 100)], 12), lambda t: (36, 30), na=12)  # ischium
    mesh = raw.mesh()
    voxel(mesh, 8.0, close=20.0, smooth_n=3)
    noise(mesh, 3.0, 0.02, seed)
    save(mesh, name)


def longboat(name, seed):
    """A Rime-folk whaling boat (the Frozen Whaler): a clinker hull 19 m long and 4.6 m in the beam, seven strakes a side
    each lapped over the one below, the sheer rising to the ends, an inside of its own (ribs, thwarts), the keel, the
    stems curling up (the prow's into a scroll). Keel along X, the prow at +X, origin on the keel amidships."""
    raw = Raw()
    L, B, D, NS, STRAKES = 1900.0, 230.0, 150.0, 60, 7
    top_c = (1.0 - math.cos(math.pi / 2 * 1.05)) ** 0.9

    def skin(s, v):
        b = B * max(0.0, 1.0 - abs(s) ** 2.4) ** 0.55
        d = D + 90.0 * abs(s) ** 3
        phi = v * math.pi / 2 * 1.05
        return (L / 2 * s, b * math.sin(phi) ** 0.8, 25.0 * s * s + d * (1.0 - math.cos(phi)) ** 0.9 / top_c)

    def normal(s, v):
        a, b = skin(s, max(0.0, v - 0.01)), skin(s, min(1.0, v + 0.01))
        ty, tz = b[1] - a[1], b[2] - a[2]
        ln = math.hypot(ty, tz) or 1.0
        return (0.0, tz / ln, -ty / ln)
    vs = []
    for k in range(STRAKES):
        for f in (0.003, 0.5, 0.997):
            vs.append(((k + f) / STRAKES, 4.5 * (1.0 - f)))
    vs[0] = (0.0, 0.0)
    vs[-1] = (1.0, 0.0)
    for sign in (1.0, -1.0):
        outer, inner = [], []
        for i in range(NS + 1):
            s = -1.0 + 2.0 * i / NS
            fade = 1.0 - abs(s) ** 4
            ro, ri = [], []
            for v, lap in vs:
                p, n = skin(s, v), normal(s, v)
                ro.append(raw.pt((p[0], sign * (p[1] + n[1] * lap * fade), p[2] + n[2] * lap * fade)))
            for q in range(15):
                p, n = skin(s, q / 14.0), normal(s, q / 14.0)
                ri.append(raw.pt((p[0], sign * max(0.0, p[1] - n[1] * 7.0), p[2] - n[2] * 7.0)))
            outer.append(ro)
            inner.append(ri)
        raw.grid(outer, lambda c: (0.0, c[1], c[2] - 200.0), wrap=False)
        raw.grid(inner, lambda c: (0.0, -c[1], 200.0 - c[2]), wrap=False)
        raw.grid([[r[-1] for r in outer], [r[-1] for r in inner]], lambda c: (0.0, 0.0, 1.0), wrap=False)
    # the keel
    tube(raw, [(L / 2 * s, 0.0, 25.0 * s * s - 12.0) for s in [-1.0 + 2.0 * i / 30 for i in range(31)]], lambda t: (16.0, 12.0), na=10)
    # the stems: the prow scrolls over, the stern rises and turns in
    tube(raw, spline([(940, 0, 20), (1000, 0, 110), (1040, 0, 240), (1056, 0, 380), (1036, 0, 500), (986, 0, 566), (936, 0, 548), (922, 0, 496)], 30),
         lambda t: (13.0, 22.0 - 8.0 * t), na=10, side=lambda p, T: (0.0, 1.0, 0.0))
    tube(raw, spline([(-940, 0, 20), (-1000, 0, 120), (-1032, 0, 260), (-1032, 0, 345), (-1004, 0, 396)], 18),
         lambda t: (13.0, 22.0 - 6.0 * t), na=10, side=lambda p, T: (0.0, 1.0, 0.0))
    # ribs (frames) inside, from gunwale to gunwale
    for s in [-0.75 + 0.1875 * k for k in range(9)]:
        path = []
        for q in range(-10, 11):
            v = abs(q) / 10.0
            p, n = skin(s, v), normal(s, v)
            path.append((p[0], (1.0 if q >= 0 else -1.0) * max(0.0, p[1] - n[1] * 14.0), p[2] - n[2] * 14.0))
        tube(raw, path, lambda t: (9.0, 7.0), na=8, side=lambda p, T: (1.0, 0.0, 0.0))
    mesh = raw.mesh()
    # thwarts across
    for x in (-600.0, -300.0, 0.0, 300.0, 600.0):
        s = x / (L / 2)
        w = 2 * skin(s, 0.86)[1]
        GS.append_box(mesh, opts(), xf((x, 0, skin(s, 0.86)[2] - 10)), 34.0, w, 10.0, 2, 4, 1, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    save(mesh, name)


def wisp_spire(name, seed):
    """Ignessa's Wisp Spire: nine hexagonal drums of fused basalt, each turned 7 degrees on the one below and a little off
    true, narrowing from 6 m across to 1.3 m, worn in at every joint, and a split crown of two leaning prongs. Origin at
    its foot (it runs 0.6 m into the ground); the drums' top about 24 m up."""
    rnd = random.Random(seed)
    raw = Raw()
    z = -60.0
    off = (0.0, 0.0)
    for d in range(9):
        r0 = 300.0 - 220.0 * d / 8.0
        r1 = 300.0 - 220.0 * (d + 1) / 8.0 if d < 8 else r0 * 0.82
        h = 330.0 if d == 0 else 270.0
        off = (off[0] + rnd.uniform(-7, 7), off[1] + rnd.uniform(-7, 7))
        a0 = math.radians(7.0 * d)
        base = [(off[0] + math.cos(a0 + math.pi / 3 * k) * r0, off[1] + math.sin(a0 + math.pi / 3 * k) * r0) for k in range(6)]
        zt = z + h
        prism(raw, rnd, seed + d, base, z, lambda x, y, zt=zt, d=d: zt + 6.0 * fbm(x / 50, y / 50, d, seed), nz=8, nw=4, mag=13.0,
              profile=lambda t, r0=r0, r1=r1: (1.0 - (1.0 - r1 / r0) * t) * (1.0 - 0.07 * (smooth(0.12, 0.0, t) + smooth(0.88, 1.0, t))),
              cap_jag=4.0)
        z = zt
    for (r, h, lean, k) in ((62.0, 360.0, (0.28, 0.1), 5), (46.0, 260.0, (-0.36, -0.12), 4)):
        c = (off[0] + lean[0] * 60, off[1] + lean[1] * 60)
        b = [(c[0] + math.cos(2 * math.pi * q / k) * r, c[1] + math.sin(2 * math.pi * q / k) * r) for q in range(k)]
        prism(raw, rnd, seed + 40 + k, b, z - 30.0, lambda x, y, h=h: z + h, nz=6, nw=2, mag=4.0, profile=lambda t: 1.0 - 0.88 * t, lean=lean, cap_jag=6.0)
    save(raw.mesh(), name)


def wisp_ribs(name, seed):
    """The spire's three glassy ribs, spiralling up it (each 400 degrees round from its foot to its crown), as their own
    mesh for the obsidian: flat bands, thinning as they climb, a little proud of the drums' faces."""
    raw = Raw()
    R = lambda z: 300.0 - 220.0 * max(0.0, min(1.0, (z + 60.0) / 2220.0))
    radial = lambda p: v_norm((p[0], p[1], 0.0))
    for r in range(3):
        path = []
        for k in range(73):
            T = k / 72.0
            a = math.radians(120.0 * r + 400.0 * T)
            z = 30.0 + 2250.0 * T
            rr = R(z) * 0.9 + 8.0
            path.append((math.cos(a) * rr, math.sin(a) * rr, z))
        tube(raw, path, lambda t: (40.0 - 18.0 * t, 20.0 - 8.0 * t), na=10, side=lambda p, T: v_cross(T, radial(p)),
             disp=lambda p, t, th: 3.0 * fbm(p[0] / 40, p[1] / 40, p[2] / 40, seed + r))
    save(raw.mesh(), name)


def glasswall(name, seed):
    """The Glasswall: a cliff of obsidian 52 m long and up to 46 m high, fused slabs leaning out over the valley (+X),
    their faces broken into shell-shaped scallops (glass's conchoidal fracture, rippled) with sharp ridges between, deep
    joints between the slabs, a ragged top; the middle slab plain 8.5-14.5 m up, where the Mirror Room's door is set.
    Origin at the foot of the middle (its face there at x = 0 near the ground); it runs 1.5 m into the ground."""
    rnd = random.Random(seed)
    raw = Raw()
    W, NY, NZ = 5200.0, 176, 150
    bounds = [-W / 2]
    while bounds[-1] < W / 2 - 300:
        bounds.append(bounds[-1] + rnd.uniform(280, 470))
    bounds[-1] = W / 2
    slabs = []
    for i in range(len(bounds) - 1):
        yc = (bounds[i] + bounds[i + 1]) / 2
        bell = math.cos(min(1.0, abs(yc) / (W / 2)) * math.pi / 2)
        slabs.append({"y0": bounds[i], "y1": bounds[i + 1], "yc": yc, "H": 1500 + 3100 * bell * rnd.uniform(0.75, 1.0),
                      "lean": rnd.uniform(0.05, 0.15), "off": rnd.uniform(-90, 90), "slope": rnd.uniform(-0.5, 0.5),
                      "peak": yc + rnd.uniform(-0.3, 0.3) * (bounds[i + 1] - bounds[i]), "drop": rnd.uniform(180, 420)})
    mid = next(s for s in slabs if s["y0"] <= 0 <= s["y1"])
    mid.update(H=max(mid["H"], 3800.0), lean=0.08, off=0.0)
    cells = {}
    for _ in range(300):
        sc = (rnd.uniform(-W / 2, W / 2), rnd.uniform(-100, 4700), rnd.uniform(140, 420), rnd.uniform(25, 110))
        for i in range(int((sc[0] - sc[2]) // 400), int((sc[0] + sc[2]) // 400) + 1):
            for j in range(int((sc[1] - sc[2]) // 400), int((sc[1] + sc[2]) // 400) + 1):
                cells.setdefault((i, j), []).append(sc)

    def slab_at(y):
        for s in slabs:
            if y <= s["y1"]:
                return s
        return slabs[-1]

    def top(y):
        s = slab_at(y)
        half = (s["y1"] - s["y0"]) / 2
        return (s["H"] - s["drop"] * (abs(y - s["peak"]) / half) ** 1.3 + s["slope"] * (y - s["yc"])
                + 160.0 * fbm(y / 110, 0, 0, seed + 3) + 60.0 * fbm(y / 35, 0, 1, seed + 3))

    def front(y, z):
        s = slab_at(y)
        x = s["off"] + s["lean"] * max(z, 0.0)
        best, rip = 0.0, 0.0
        for (cy, cz, r, dep) in cells.get((int(y // 400), int(z // 400)), ()):
            d = math.hypot(y - cy, z - cz)
            if d < r:
                v = dep * (1 - (d / r) ** 2)
                if v > best:
                    best, rip = v, 2.5 * math.sin(d / 13.0 * 2 * math.pi) * (d / r)
        door = (1.0 - smooth(260.0, 360.0, abs(y))) * (1.0 - smooth(300.0, 420.0, abs(z - 1150.0)))
        x -= (best + rip) * (1.0 - door)
        for b in bounds[1:-1]:
            x -= 75.0 * math.exp(-((y - b) / 22.0) ** 2)
        return x + 14.0 * fbm(y / 220, z / 220, 0, seed + 4) * (1.0 - door)

    back = lambda y, z: -1000.0 + 0.18 * z + 60.0 * fbm(y / 300, z / 300, 1, seed + 5)
    F, Bk = [], []
    for i in range(NZ + 1):
        rf, rb = [], []
        for j in range(NY + 1):
            y = -W / 2 + W * j / NY
            z = -150.0 + (top(y) + 150.0) * i / NZ
            rf.append(raw.pt((front(y, z), y, z)))
            rb.append(raw.pt((back(y, z), y, z)))
        F.append(rf)
        Bk.append(rb)
    raw.grid(F, lambda c: (1.0, 0.0, 0.0), wrap=False)
    raw.grid(Bk, lambda c: (-1.0, 0.0, 0.0), wrap=False)
    # the ragged top between them
    crest = []
    for j in range(NY + 1):
        pf, pb = raw.pts[F[-1][j]], raw.pts[Bk[-1][j]]
        crest.append(raw.pt(((pf[0] + pb[0]) / 2, pf[1], pf[2] + 60.0 + 90.0 * fbm(pf[1] / 90, 0, 2, seed + 6))))
    raw.grid([F[-1], crest, Bk[-1]], lambda c: (0.0, 0.0, 1.0), wrap=False)
    for j, sgn in ((0, -1.0), (NY, 1.0)):
        raw.grid([[F[i][j], Bk[i][j]] for i in range(NZ + 1)], lambda c, sgn=sgn: (0.0, sgn, 0.0), wrap=False)
    save(raw.mesh(), name)


def quarry_face(name, seed):
    """Warden's Quarry: an outcrop of obsidian 14 m long with its front (+X) cut back in three benches, the cut faces
    scored by the drill channels the blocks were split along, the benches' tops tooled; natural rock over the top and
    behind, falling away into the ground at both ends; under it all a skirt of rock 15 m deep (it stands on a hilltop
    whose ground falls away round it: set on the highest ground along its front, the skirt meets the ground anywhere).
    Origin at the foot of the front, its middle; 6 m high at most; its ends closed."""
    raw = Raw()
    Wd = 1400.0
    cut = [(0, -1600), (0, -120), (0, 160), (-220, 160), (-220, 340), (-450, 340), (-450, 470), (-600, 560), (-800, 600),
           (-1000, 520), (-1150, 300), (-1250, -120), (-1250, -1600)]
    kinds = ["skirt", "face", "tread", "face", "tread", "face", "rock", "rock", "rock", "rock", "rock", "skirt"]
    crag = lambda u: (-600.0 * u, -120.0 + 680.0 * math.sin(u * math.pi / 2) ** 0.8)   # the natural slope where nothing was cut
    NYQ = 176
    ends = ([], [])
    for k in range(len(cut) - 1):
        (x0, z0), (x1, z1) = cut[k], cut[k + 1]
        segs = max(2, int(math.hypot(x1 - x0, z1 - z0) / 30))
        nx, nz = (z1 - z0), -(x1 - x0)
        ln = math.hypot(nx, nz)
        nx, nz = nx / ln, nz / ln
        if k == 0:
            nx, nz = 1.0, 0.0
        elif k == len(cut) - 2:
            nx, nz = -1.0, 0.0
        elif nx * 1.0 + nz * 0.2 < 0 and kinds[k] != "rock":
            nx, nz = -nx, -nz
        ids = []
        for i in range(segs + 1):
            t = i / segs
            row = []
            for j in range(NYQ + 1):
                y = -Wd / 2 + Wd * j / NYQ
                x, z = x0 + (x1 - x0) * t, z0 + (z1 - z0) * t
                w = 1.0 - smooth(380.0, 620.0, abs(y) + 60.0 * fbm(y / 200, 0, 3, seed))
                if 1 <= k <= 5:
                    u = (k - 1 + t) / 5.0
                    cx, cz = crag(u)
                    x, z = cx + (x - cx) * w, cz + (z - cz) * w
                edgef = math.sin(math.pi * t) ** 0.5
                if kinds[k] == "face":
                    d = -4.5 * (0.5 - 0.5 * math.cos(2 * math.pi * y / 34.0)) * w + 1.5 * fbm(y / 30, z / 30, 0, seed)
                elif kinds[k] == "tread":
                    d = -1.2 * (0.5 - 0.5 * math.cos(2 * math.pi * x / 22.0)) * w + 1.0 * fbm(x / 30, y / 30, 1, seed)
                elif kinds[k] == "skirt":
                    d = 30.0 * fbm(x / 200, y / 200, z / 200, seed + 5) + 10.0 * fbm(x / 60, y / 60, z / 60, seed + 6)
                else:
                    d = 55.0 * fbm(x / 300, y / 300, z / 300, seed + 2) + 18.0 * fbm(x / 90, y / 90, z / 90, seed + 3) + 6.0 * fbm(x / 30, y / 30, z / 30, seed + 4)
                if kinds[k] in ("face", "tread") and w < 1.0:
                    d = d * w + (1.0 - w) * (34.0 * fbm(x / 260, y / 260, z / 260, seed + 2))
                d *= edgef
                end = 1.0 - smooth(560.0, Wd / 2, abs(y))
                zz = z + d * nz
                if zz > -120.0:
                    zz = -120.0 + (zz + 120.0) * (0.15 + 0.85 * end)
                p = (x + d * nx, y, zz)
                row.append(raw.pt(p))
                if j == 0:
                    ends[0].append(p)
                elif j == NYQ:
                    ends[1].append(p)
            ids.append(row)
        raw.grid(ids, lambda c, nx=nx, nz=nz: (nx, 0.0, nz), wrap=False)
    # its ends closed (the section there, fanned from its middle)
    for pts, sgn in ((ends[0], -1.0), (ends[1], 1.0)):
        cx = sum(p[0] for p in pts) / len(pts)
        cz = sum(p[2] for p in pts) / len(pts)
        ring = [raw.pt(p) for p in pts]
        c = raw.pt((cx, pts[0][1], cz))
        for q in range(len(ring)):
            raw.face(c, ring[q], ring[(q + 1) % len(ring)], (0.0, sgn, 0.0))
    save(raw.mesh(), name)


def cut_block(name, seed, sx, sy, sz):
    """A block split out of the obsidian (Warden's Quarry): dressed flat but not true, chisel grooves across its top, a
    row of wedge holes along one top edge, its corners knocked off. Origin at the centre of its foot."""
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    GS.append_box(mesh, opts(), xf((0, 0, 0)), sx, sy, sz, 28, 24, 20, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    Q, E = unreal.GeometryScript_MeshQueries, unreal.GeometryScript_MeshEdits
    for v in range(Q.get_vertex_count(mesh)):
        p, ok = Q.get_vertex_position(mesh, v)
        x, y, z = p.x, p.y, p.z
        ux, uy, uz = abs(x) / (sx / 2), abs(y) / (sy / 2), z / sz
        dz = 0.0
        if z > sz - 0.5:
            for g in (-0.3, 0.0, 0.3):
                dz -= 2.5 * math.exp(-((y - g * sy) / 3.0) ** 2)
            if y > sy / 2 - 18 and ((x + sx / 2) % 26) < 10:
                dz -= 8.0
        # the corners and edges knocked back toward the middle
        k = max(0.0, ux + uy - 1.75) * 0.5 + max(0.0, ux + uz - 1.8) * 0.4 + max(0.0, uy + uz - 1.8) * 0.4
        k += 0.02 * (fbm(x / 40, y / 40, z / 40, seed) + 1.0)
        E.set_vertex_position(mesh, v, unreal.Vector(x * (1 - k * 0.6), y * (1 - k * 0.6), z * (1 - k * 0.25) + dz), True)
    noise(mesh, 1.5, 0.04, seed)
    save(mesh, name)


def golem_rough(name, seed):
    """The Warden's Quarry's second golem, left half carved: a rough block of the obsidian, its top worked down into a
    figure lying face up (the head toward +X): a broad chest, the head, the shoulders, arms laid along its sides, the hips;
    its legs not yet cut out of the block's -X end. Parts voxel-merged so the figure grows out of the stone; the block's
    sides left rough. Origin near the centre of its foot, 4.9 m long."""
    mesh = unreal.DynamicMesh()
    GS.append_box(mesh, opts(), xf((-5, 0, 0)), 490, 250, 50, 8, 6, 3, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    GS.append_box(mesh, opts(), xf((-190, 0, 0)), 120, 250, 135, 4, 6, 4, unreal.GeometryScriptPrimitiveOriginMode.BASE)   # the uncut end
    raw = Raw()
    E = lambda c, r: ellipsoid(raw, c, r, nu=12, nv=18)
    E((25, 0, 92), (115, 72, 42))          # chest
    E((-55, 0, 82), (62, 58, 30))          # belly
    E((-125, 0, 78), (55, 70, 30))         # hips, going into the stone
    E((178, 0, 98), (50, 42, 42))          # head
    tube(raw, [(128, 0, 92), (150, 0, 95)], lambda t: (20, 18), na=10)       # neck
    for s_ in (-1, 1):
        E((85, s_ * 90, 92), (44, 38, 34))                                     # shoulders
        tube(raw, spline([(90, s_ * 118, 84), (20, s_ * 126, 74), (-50, s_ * 124, 70), (-100, s_ * 118, 70)], 10),
             lambda t: (22 - 4 * t, 19 - 3 * t), na=10)                         # arms laid along its sides, clear of it
        E((-108, s_ * 116, 68), (25, 21, 18))                                  # the fists
    append(mesh, raw)
    voxel(mesh, 5.0, close=7.0, smooth_n=1)
    noise(mesh, 3.0, 0.035, seed)
    noise(mesh, 1.0, 0.12, seed + 1)
    save(mesh, name)


def crag(name, seed, soft=False):
    """A panel of jointed rock for a steep face (the dressing lays them against it, leaning back to its slope): columns of
    blocks split along their joints, each block's face set back its own way, the columns of every height, the top ragged
    and stepped back; merged into one rock, the faces left angular (a coarse voxel close, no smoothing). Front toward +X,
    its back flat at X -220 (buried in the slope), 10 m across (Y), up to 8.5 m tall, origin at the middle of its foot.
    soft: the same blocks with snow and ice built up over them, every edge rounded (a wide close, relaxed): the Tundra's
    faces (the angular ones in pale ice read as sugar cubes)."""
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    y = -500.0
    while y < 500.0:
        w = rnd.uniform(140.0, 260.0)
        w = min(w, 500.0 - y)
        if w < 60.0:
            break
        top = rnd.uniform(420.0, 850.0) * (0.8 + 0.2 * math.sin((y + 500.0) / 1000.0 * math.pi))
        z, front = 0.0, rnd.uniform(160.0, 260.0)
        while z < top - 40.0:
            h = min(rnd.uniform(120.0, 300.0), top - z)
            # each block of the column sits a little back or proud of the one below; higher up the face steps back
            front = max(60.0, min(280.0, front + rnd.uniform(-60.0, 30.0) - 10.0))
            depth = front + 220.0
            yaw, pitch = rnd.uniform(-5.0, 5.0), rnd.uniform(-4.0, 4.0)
            GS.append_box(mesh, opts(), xf((front - depth / 2.0, y + w / 2.0 + rnd.uniform(-10.0, 10.0), z - 8.0), (pitch, yaw, rnd.uniform(-3.0, 3.0))),
                          depth, w - rnd.uniform(0.0, 18.0), h + 16.0, 1, 1, 1, unreal.GeometryScriptPrimitiveOriginMode.BASE)
            z += h
        y += w
    # a few loose blocks fallen onto the foot
    for _ in range(rnd.randint(2, 4)):
        s = rnd.uniform(60.0, 120.0)
        GS.append_box(mesh, opts(), xf((rnd.uniform(220.0, 320.0), rnd.uniform(-420.0, 420.0), -10.0), (rnd.uniform(-15, 15), rnd.uniform(0, 90), rnd.uniform(-15, 15))),
                      s, s * rnd.uniform(0.7, 1.2), s * rnd.uniform(0.5, 0.9), 1, 1, 1, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    if soft:
        voxel(mesh, 12.0, close=55.0, smooth_n=6, tris=50000)
        noise(mesh, 14.0, 0.008, seed)
        noise(mesh, 2.0, 0.04, seed + 1)
    else:
        voxel(mesh, 9.0, close=14.0, smooth_n=0, tris=60000)
        noise(mesh, 9.0, 0.012, seed)
        noise(mesh, 3.0, 0.05, seed + 1)
    save(mesh, name, nanite=True)


def pebbles(name, seed, n=170, spread=165.0):
    """Beach pebbles (round 7, the coast's beach_pebbles): a strip of stones the sea has worn round and flat, about 5.6 m
    along the shore (X) and 1.8 m across, 6-40 cm, bigger and closer along its middle, thinning out raggedly at its ends
    and edges (a round patch read as a blot), each lying flat its own way and a third sunk in the sand, their skins a
    little uneven. Origin at the middle of the strip on the sand; Nanite."""
    rnd = random.Random(seed)
    raw = Raw()
    for k in range(n):
        r = spread * rnd.random() ** 0.8
        a = rnd.uniform(0, 2 * math.pi)
        big = max(0.0, 1.0 - r / (spread * 1.1))
        rx = rnd.uniform(3.0, 8.0) + 12.0 * big * rnd.random() ** 1.5
        ry, rz = rx * rnd.uniform(0.6, 1.0), rx * rnd.uniform(0.32, 0.55)
        yaw, tip = rnd.uniform(0, 2 * math.pi), math.radians(rnd.uniform(-12.0, 12.0))
        ax = ((math.cos(yaw) * math.cos(tip), math.sin(yaw) * math.cos(tip), math.sin(tip)), (-math.sin(yaw), math.cos(yaw), 0.0),
              (-math.cos(yaw) * math.sin(tip), -math.sin(yaw) * math.sin(tip), math.cos(tip)))
        sd = rnd.randint(1, 1 << 20)
        px, py = math.cos(a) * r * 1.7, math.sin(a) * r * 0.55
        py += 28.0 * math.sin(px / 140.0 + seed)
        ellipsoid(raw, (px, py, rz * 0.32), (rx, ry, rz), ax, nu=7, nv=12,
                  disp=lambda p, sd=sd, rx=rx: rx * 0.09 * fbm(p[0] / 7.0, p[1] / 7.0, p[2] / 7.0, sd, 2))
    save(raw.mesh(), name, True)


def driftwood(name, seed, length, roots=True, stubs=2):
    """A drift log cast up by the sea (round 7, the coast's driftwood): a trunk worn smooth and split along its grain
    (shallow grooves), bent a little, thinning to a snapped end; at its foot the root plate, roots splayed and broken
    short (roots=False: a length of trunk snapped at both ends), a branch stub or two. Lying along X, the foot at -X,
    its underside a little sunk; origin under its middle on the sand. Nanite."""
    rnd = random.Random(seed)
    raw = Raw()
    r0 = rnd.uniform(15.0, 21.0) * (0.75 + 0.25 * length / 400.0)
    L = length
    ctrl = [(-L / 2, 0.0, r0 * 0.72), (-L / 6, rnd.uniform(-22, 22), r0 * 0.72 + rnd.uniform(-3, 5)),
            (L / 6, rnd.uniform(-30, 30), r0 * 0.6 + rnd.uniform(-3, 5)), (L / 2, rnd.uniform(-45, 45), r0 * 0.4)]
    path = spline(ctrl, 44)
    flare = 0.45 if roots else 0.0

    def radius(t):
        r = r0 * (1.0 - 0.5 * t) * (1.0 + flare * (1.0 - smooth(0.0, 0.12, t)))
        return (r, r * 0.92)

    def grain(p, t, th, sd=seed):
        g = math.sin(th * 7.0 + 2.0 * fbm(p[0] / 120.0, t * 3.0, 0.3, sd, 2))
        return -2.4 * max(0.0, g) ** 6 + 1.2 * fbm(p[0] / 18.0, p[1] / 18.0, p[2] / 18.0, sd + 1, 2)
    tube(raw, path, radius, na=20, disp=grain, caps=True, jag=(rnd, r0 * 0.9))
    if roots:
        # the root plate: a bole wider than the trunk, thick roots out of its rim, short (broken), curling back
        foot = path[0]
        ellipsoid(raw, v_add(foot, (-r0 * 0.15, 0.0, 0.0)), (r0 * 0.7, r0 * 1.45, r0 * 1.35), nu=10, nv=16,
                  disp=lambda p, sd=seed + 50: r0 * 0.12 * fbm(p[0] / 14.0, p[1] / 14.0, p[2] / 14.0, sd, 2))
        # (round 7: splayed all round, long and thin, they read as a starfish of bones from the beach)
        n = rnd.randint(4, 6)
        for k in range(n):
            ph = 2 * math.pi * (k + rnd.uniform(-0.3, 0.3)) / n
            d = v_norm((-rnd.uniform(0.45, 0.8), math.cos(ph), math.sin(ph)))
            start = v_add(foot, v_mul(d, r0 * 0.7))
            ln = rnd.uniform(22.0, 48.0) * r0 / 18.0
            bend = (-0.3, rnd.uniform(-0.15, 0.15), -0.18)
            pts = [v_add(start, v_add(v_mul(d, ln * u), v_mul(bend, ln * u * u))) for u in (0.0, 0.35, 0.7, 1.0)]
            rr = r0 * rnd.uniform(0.42, 0.62)
            tube(raw, spline(pts, 9), lambda t, rr=rr: (rr * (1.0 - 0.72 * t) + 1.5, rr * (0.8 - 0.55 * t) + 1.5), na=12,
                 disp=lambda p, t, th, sd=seed + k: 1.4 * fbm(p[0] / 10.0, p[1] / 10.0, p[2] / 10.0, sd, 2), caps=True, jag=(rnd, rr * 0.5))
    for k in range(stubs):
        i = int(rnd.uniform(0.3, 0.8) * (len(path) - 1))
        base = path[i]
        ph = rnd.uniform(-1.0, 1.0) * math.pi * 0.45 + (math.pi / 2 if rnd.random() < 0.5 else -math.pi / 2)
        d = v_norm((0.6, math.cos(ph), abs(math.sin(ph)) * 0.6 + 0.2))
        rr = radius(i / (len(path) - 1))[0] * 0.35
        ln = rnd.uniform(25.0, 60.0)
        tube(raw, [v_add(base, v_mul(d, ln * u)) for u in (0.0, 0.5, 1.0)], lambda t, rr=rr: (rr * (1.0 - 0.4 * t), rr * (1.0 - 0.4 * t)), na=10,
             caps=True, jag=(rnd, rr * 0.7))
    save(raw.mesh(), name, True)


def wrack(name, seed):
    """The tide's strand line (round 7, the coast's wrack): dark tangles of weed along a wavy line about 5.4 m long (X),
    thick here and thinning to nothing there, a few long straps of kelp lying out of it; a few cm high. Origin at its
    middle on the sand. Nanite."""
    rnd = random.Random(seed)
    raw = Raw()
    L = 540.0
    for k in range(170):
        u = rnd.uniform(-L / 2, L / 2)
        dens = 0.5 + 0.5 * fbm(u / 90.0, 0.31, 0.77, seed, 2)
        if rnd.random() > 0.15 + 0.85 * dens:
            continue
        y = 28.0 * math.sin(u / 120.0 + seed) + 14.0 * fbm(u / 40.0, 1.1, 0.2, seed + 1, 2) + rnd.gauss(0.0, 9.0)
        sx = rnd.uniform(5.0, 16.0) * (0.6 + 0.6 * dens)
        yaw = rnd.uniform(0, 2 * math.pi)
        ax = ((math.cos(yaw), math.sin(yaw), 0.0), (-math.sin(yaw), math.cos(yaw), 0.0), (0.0, 0.0, 1.0))
        rz = rnd.uniform(1.0, 2.6) * (0.7 + 0.5 * dens)
        # (crinkled: smooth flattened blobs read as a string of olives up close)
        ellipsoid(raw, (u, y, rz * 0.25), (sx, sx * rnd.uniform(0.4, 0.8), rz), ax, nu=6, nv=14,
                  disp=lambda p, sd=seed + k, sx=sx: 0.22 * sx * fbm(p[0] / 2.5, p[1] / 2.5, p[2] / 2.5, sd, 3) + 1.0 * fbm(p[0] / 6.0, p[1] / 6.0, 0.4, sd + 9, 2))
    for k in range(rnd.randint(9, 14)):
        u0 = rnd.uniform(-L / 2 + 40, L / 2 - 40)
        a = rnd.uniform(0, 2 * math.pi)
        ln = rnd.uniform(60.0, 170.0)
        pts = []
        x, y = u0, 28.0 * math.sin(u0 / 120.0 + seed)
        for m in range(7):
            pts.append((x, y, 1.1 + 0.4 * math.sin(m * 1.7)))
            a += rnd.uniform(-0.5, 0.5)
            x += math.cos(a) * ln / 6.0
            y += math.sin(a) * ln / 6.0
        w = rnd.uniform(2.5, 5.0)
        tube(raw, spline(pts, 18), lambda t, w=w: (w * (1.0 - 0.5 * t) + 0.8, 0.55), na=8,
             side=lambda p, T: v_cross(T, (0.0, 0.0, 1.0)), caps=True)
    save(raw.mesh(), name, True)


def ice_face(name, seed):
    """A glacier's or an ice shelf's face (round 7, the Tundra's steep ice: the jointed panels, even rounded over, read as
    blocks stuck on the wall): one sheet of ice 12 m across and up to 10 m tall, fluted from top to bottom (rounded
    ribs and runnels that wander a little), set back a step where each of its layers ends, its top rounding over (now
    and then a cornice of snow leaning out), its foot flared; its sides and top sink back into the slope, so faces laid
    overlapping join without an edge. Front toward +X, back flat at X -250 (buried), origin at the middle of its foot,
    as crag()."""
    rnd = random.Random(seed)
    raw = Raw()
    W, NY, NV = 600.0, 97, 73
    htop = rnd.uniform(800.0, 980.0)
    cornice = rnd.random() < 0.55
    waves = [(rnd.uniform(70.0, 240.0), rnd.uniform(0, 2 * math.pi), rnd.uniform(0.5, 1.0)) for _ in range(4)]
    wsum = sum(w[2] for w in waves)
    bands = sorted(rnd.uniform(90.0, htop - 140.0) for _ in range(rnd.randint(3, 6)))

    def top(y):
        return htop * (0.8 + 0.2 * (0.5 + 0.5 * fbm(y / 260.0, 0.5, 0.5, seed, 2))) * (1.0 - 0.22 * (y / W) ** 2)

    def front(y, z, zt):
        keep = smooth(0.0, 190.0, W - abs(y)) * (1.0 - smooth(zt - 170.0, zt, z)) * smooth(-60.0, 10.0, z)
        rib = 0.0
        for lam, ph, amp in waves:
            sv = math.sin(2 * math.pi * y / lam + ph + 0.7 * fbm(z / 320.0, y / 400.0, 0.1, seed + 3, 2))
            rib += amp * (1.0 - abs(sv)) ** 1.6
        x = 150.0 - 45.0 * z / htop + 46.0 * rib / wsum
        x -= sum(13.0 * smooth(b - 8.0, b + 8.0, z) for b in bands)
        if cornice:
            x += 75.0 * math.exp(-((z - (zt - 70.0)) / 55.0) ** 2) * (0.6 + 0.4 * fbm(y / 150.0, 2.2, 0.4, seed + 7, 2))
        x += 55.0 * (1.0 - smooth(-60.0, 70.0, z))
        x += 9.0 * fbm(y / 60.0, z / 60.0, 0.3, seed + 5, 3) + 3.0 * fbm(y / 14.0, z / 14.0, 0.8, seed + 6, 2)
        return keep * x + (1.0 - keep) * -30.0, keep

    F, B = [], []
    for i in range(NV):
        rf, rb = [], []
        v = i / (NV - 1.0)
        for j in range(NY):
            y = -W + 2.0 * W * j / (NY - 1.0)
            zt = top(y)
            z = -60.0 + v * (zt + 60.0)
            fx, keep = front(y, z, zt)
            rf.append(raw.pt((fx, y, z)))
            # the back follows the front where it thins out (its sides, top and foot): no flat side shows where the
            # face stands out of the slope (round 7: those flat cuts read as slabs)
            rb.append(raw.pt((fx - 230.0 * keep - 2.0, y, z)))
        F.append(rf)
        B.append(rb)
    raw.grid(F, lambda c: (1.0, 0.0, 0.0), wrap=False)
    raw.grid(B, lambda c: (-1.0, 0.0, 0.0), wrap=False)
    raw.grid([F[-1], B[-1]], lambda c: (0.0, 0.0, 1.0), wrap=False)
    raw.grid([F[0], B[0]], lambda c: (0.0, 0.0, -1.0), wrap=False)
    raw.grid([[r[0] for r in F], [r[0] for r in B]], lambda c: (0.0, -1.0, 0.0), wrap=False)
    raw.grid([[r[-1] for r in F], [r[-1] for r in B]], lambda c: (0.0, 1.0, 0.0), wrap=False)
    save(raw.mesh(), name, nanite=True)


def _frame(d):
    """Two unit vectors square to d and to each other (an ellipsoid's or a strap's axes about a stalk)."""
    u = v_norm(v_cross(d, (0.0, 0.0, 1.0) if abs(d[2]) < 0.95 else (1.0, 0.0, 0.0)))
    return u, v_cross(d, u)


def crop(name, seed, kind):
    """A clump of a field's crop (round 8, the meadow's fields: each crop was eight to twelve 1 cm boxes with a box for an
    ear, a field of sticks). wheat: ~50 stalks about a metre tall in a clump 0.9 m across, leaning out a little, each with
    its ear (a slim head) and a leaf or two low down; barley: shorter, its ears nodding over with long awns; cabbage: a
    head in a rosette of cupped outer leaves 45 cm across. Origin on the ground at the middle; Nanite."""
    rnd = random.Random(seed)
    raw = Raw()
    if kind in ("wheat", "barley"):
        wheat = kind == "wheat"
        n = 52 if wheat else 46
        for k in range(n):
            r = 44.0 * rnd.random() ** 0.6
            a = rnd.uniform(0, 2 * math.pi)
            x, y = math.cos(a) * r, math.sin(a) * r
            h = rnd.uniform(84.0, 108.0) if wheat else rnd.uniform(68.0, 88.0)
            lean = math.radians(rnd.uniform(1.0, 7.0) + r / 44.0 * 6.0)
            la = a + rnd.uniform(-0.6, 0.6)
            d = v_norm((math.cos(la) * math.sin(lean), math.sin(la) * math.sin(lean), math.cos(lean)))
            base = (x, y, -3.0)
            top = v_add(base, v_mul(d, h))
            mid = v_add(v_add(base, v_mul(d, h * 0.5)), (rnd.uniform(-1.5, 1.5), rnd.uniform(-1.5, 1.5), 0.0))
            tube(raw, spline([base, mid, top], 5), lambda t: (0.34 * (1.0 - 0.35 * t), 0.34 * (1.0 - 0.35 * t)), na=4, caps=False)
            if wheat:
                u, v = _frame(d)
                ellipsoid(raw, v_add(top, v_mul(d, 4.2)), (0.75, 0.55, 4.6), (u, v, d), nu=5, nv=6)
            else:
                # the ear nods over: hangs out and down from the top of the stalk, its awns trailing past it
                nod = v_norm(v_add(v_mul((math.cos(la), math.sin(la), 0.0), 1.0), (0.0, 0.0, rnd.uniform(-0.9, -0.3))))
                u, v = _frame(nod)
                c = v_add(top, v_mul(nod, 3.6))
                ellipsoid(raw, c, (0.6, 0.5, 3.8), (u, v, nod), nu=5, nv=6)
                for w in range(5):
                    off = v_add(v_mul(u, rnd.uniform(-0.5, 0.5)), v_mul(v, rnd.uniform(-0.5, 0.5)))
                    p0 = v_add(c, off)
                    p1 = v_add(p0, v_mul(v_norm(v_add(nod, (rnd.uniform(-0.15, 0.15), rnd.uniform(-0.15, 0.15), -0.1))), rnd.uniform(7.0, 10.0)))
                    tube(raw, [p0, p1], lambda t: (0.09, 0.09), na=3, caps=False)
            for _ in range(rnd.randint(1, 2)):
                # a leaf: a strap from low on the stalk arching out and down
                z0 = rnd.uniform(0.15, 0.4) * h
                p0 = v_add(base, v_mul(d, z0))
                out = (math.cos(la + rnd.uniform(-1.2, 1.2)), math.sin(la + rnd.uniform(-1.2, 1.2)), 0.0)
                ln = rnd.uniform(16.0, 26.0)
                pts = [p0, v_add(p0, v_add(v_mul(out, ln * 0.45), (0.0, 0.0, ln * 0.35))), v_add(p0, v_add(v_mul(out, ln), (0.0, 0.0, ln * 0.05)))]
                tube(raw, spline(pts, 6), lambda t: (0.7 * (1.0 - 0.8 * t) + 0.08, 0.06), na=4, side=lambda p, T: (0.0, 0.0, 1.0), caps=False)
    else:
        # cabbage: a head, then two rings of cupped leaves opening out from under it
        hr = rnd.uniform(8.5, 10.5)
        ellipsoid(raw, (0.0, 0.0, hr * 0.85), (hr, hr * rnd.uniform(0.9, 1.0), hr * 0.85), nu=8, nv=12,
                  disp=lambda p, sd=seed: 0.6 * fbm(p[0] / 4.0, p[1] / 4.0, p[2] / 4.0, sd, 2))
        for ring, (cnt, rr, tilt, size) in enumerate(((6, 8.0, 58.0, 10.0), (8, 15.0, 22.0, 13.0))):
            for k in range(cnt):
                a = 2 * math.pi * (k + rnd.uniform(-0.2, 0.2) + ring * 0.5) / cnt
                out = (math.cos(a), math.sin(a), 0.0)
                t = math.radians(tilt + rnd.uniform(-8.0, 8.0))
                n_up = v_norm((math.cos(a) * math.sin(t), math.sin(a) * math.sin(t), math.cos(t)))   # the leaf's face normal
                along = v_norm(v_cross(n_up, (-math.sin(a), math.cos(a), 0.0)))                         # up the leaf, outward
                side = v_cross(n_up, along)
                sz = size * rnd.uniform(0.85, 1.15)
                c = v_add(v_mul(out, rr), (0.0, 0.0, 7.0 - ring * 4.5))
                ellipsoid(raw, c, (sz * 0.8, sz, 1.1), (side, v_mul(along, -1.0), n_up), nu=5, nv=10,
                          disp=lambda p, sd=seed + k + 10 * ring: 0.5 * fbm(p[0] / 3.0, p[1] / 3.0, p[2] / 3.0, sd, 2))
    save(raw.mesh(), name, True)


def _closed_sheet(raw, top_pts, thick):
    """A sheet given as rows of points (its top face), closed: a bottom face 'thick' under it and its edges joined."""
    T = [[raw.pt(p) for p in row] for row in top_pts]
    B = [[raw.pt((p[0], p[1], p[2] - thick)) for p in row] for row in top_pts]
    raw.grid(T, lambda c: (0.0, 0.0, 1.0), wrap=False)
    raw.grid(B, lambda c: (0.0, 0.0, -1.0), wrap=False)
    raw.grid([T[0], B[0]], lambda c: (-1.0, 0.0, 0.0), wrap=False)
    raw.grid([T[-1], B[-1]], lambda c: (1.0, 0.0, 0.0), wrap=False)
    raw.grid([[r[0] for r in T], [r[0] for r in B]], lambda c: (0.0, -1.0, 0.0), wrap=False)
    raw.grid([[r[-1] for r in T], [r[-1] for r in B]], lambda c: (0.0, 1.0, 0.0), wrap=False)


def awning(name, seed):
    """A market stall's canvas (round 8: it was a flat slab with a strip for a valance): the canvas sagging between its
    four posts and along its edges, rumpled a little, the front valance hanging from it with a scalloped hem. Fits a 100
    cm box (as the engine cube: the town's blocks scale it), centred: X across the stall, Y front (+) to back, the
    canvas at the top, the valance hanging to the bottom."""
    rnd = random.Random(seed)
    raw = Raw()
    NX, NY = 41, 25
    rows = []
    for j in range(NY):
        y = -50.0 + 100.0 * j / (NY - 1)
        row = []
        for i in range(NX):
            x = -50.0 + 100.0 * i / (NX - 1)
            sag = 9.0 * math.sin(math.pi * (x + 50.0) / 100.0) * (0.55 + 0.45 * math.sin(math.pi * (y + 50.0) / 100.0))
            row.append((x, y, 50.0 - sag + 1.2 * fbm(x / 14.0, y / 14.0, 0.3, seed, 2)))
        rows.append(row)
    _closed_sheet(raw, rows, 1.0)
    # the valance: a strip down from the front edge, its hem in scallops
    V = []
    for k in range(9):
        t = k / 8.0
        row = []
        for i in range(NX):
            x = -50.0 + 100.0 * i / (NX - 1)
            top = 50.0 - 9.0 * math.sin(math.pi * (x + 50.0) / 100.0)
            hem = -50.0 + 14.0 * (1.0 - abs(math.sin(math.pi * (x + 50.0) / 100.0 * 7.0)))
            row.append((x, 50.0 + 0.6 * math.sin(t * math.pi), top - (top - hem) * t))
        V.append(row)
    Vf = [[raw.pt(p) for p in row] for row in V]
    Vb = [[raw.pt((p[0], p[1] - 0.8, p[2])) for p in row] for row in V]
    raw.grid(Vf, lambda c: (0.0, 1.0, 0.0), wrap=False)
    raw.grid(Vb, lambda c: (0.0, -1.0, 0.0), wrap=False)
    raw.grid([Vf[-1], Vb[-1]], lambda c: (0.0, 0.0, -1.0), wrap=False)
    save(raw.mesh(), name)


def haystack(name, seed):
    """A haystack (round 8: they were yellow cylinders): a beehive of hay, its flanks bulging and ragged with loose straw,
    a rounded crown drawn up to a point, its foot spread. Fits a 100 cm box, centred, like the engine shapes."""
    rnd = random.Random(seed)
    raw = Raw()
    NV, NU = 30, 40
    rings = []
    for i in range(NV + 1):
        t = i / NV
        z = -50.0 + 100.0 * t
        r = 50.0 * (1.0 - t ** 2.4) ** 0.55 * (1.0 + 0.08 * (1.0 - smooth(0.0, 0.12, t)))
        ring = []
        for j in range(NU):
            a = 2 * math.pi * j / NU
            rr = r * (1.0 + 0.05 * fbm(math.cos(a) * 2.0, math.sin(a) * 2.0, t * 3.0, seed, 3)) + 1.6 * fbm(a * 6.0, t * 20.0, 0.5, seed + 1, 2)
            ring.append(raw.pt((math.cos(a) * rr, math.sin(a) * rr, z)))
        rings.append(ring)
    raw.grid(rings, lambda c: (c[0], c[1], 0.3))
    raw.fan(rings[0], (0.0, 0.0, -50.0), (0.0, 0.0, -1.0))
    raw.fan(rings[-1], (0.0, 0.0, 50.0), (0.0, 0.0, 1.0))
    save(raw.mesh(), name)


def rowboat(name, seed):
    """A rowboat moored at the jetty (round 8: it was three boxes): a clinker hull, its strakes lapped, sheer rising to a
    pointed bow (+X) and a small transom at the stern, open, two thwarts across it and a keel. Fits a 100 cm box,
    centred: X along the boat, Y across, Z from the keel (-50) to the bow's tip (+50)."""
    rnd = random.Random(seed)
    raw = Raw()
    NX, NA = 33, 17
    outer, inner = [], []
    for i in range(NX):
        t = i / (NX - 1)                      # 0 stern .. 1 bow
        x = -50.0 + 100.0 * t
        beam = 50.0 * (math.sin(math.pi * min(1.0, 0.18 + t * 0.92)) ** 0.75) * (0.55 + 0.45 * smooth(0.0, 0.25, t))
        depth = 62.0 * (0.75 + 0.25 * math.sin(math.pi * t))
        sheer = 38.0 + 12.0 * smooth(0.65, 1.0, t) + 4.0 * (1.0 - smooth(0.0, 0.2, t))
        ro, ri = [], []
        for j in range(NA):
            a = math.pi * j / (NA - 1)        # round the hull from port gunwale to starboard
            sy, sz = -math.cos(a), math.sin(a)
            step = 1.2 * (int(sz * 5.0) % 2)  # the strakes, each lapped over the one below
            y = sy * (beam + step)
            z = sheer - sz * depth * (0.9 if t > 0.03 else 0.7)
            ro.append(raw.pt((x, y, z)))
            ri.append(raw.pt((x, sy * max(0.0, beam - 2.6), z + 2.6 * sz)))
        outer.append(ro)
        inner.append(ri)
    raw.grid(outer, lambda c: (0.0, c[1], -0.3), wrap=False)
    raw.grid(inner, lambda c: (0.0, -c[1], 0.3), wrap=False)
    raw.grid([[r[0] for r in outer], [r[0] for r in inner]], lambda c: (0.0, -1.0, 0.4), wrap=False)   # the gunwales
    raw.grid([[r[-1] for r in outer], [r[-1] for r in inner]], lambda c: (0.0, 1.0, 0.4), wrap=False)
    raw.grid([outer[0], inner[0]], lambda c: (-1.0, 0.0, 0.0), wrap=False)                            # the transom's edge
    raw.fan(inner[0], (-50.0, 0.0, 10.0), (-1.0, 0.0, 0.0))
    raw.fan(outer[-1], (50.0, 0.0, 30.0), (1.0, 0.0, 0.0))
    mesh = raw.mesh()
    # the thwarts just under the gunwales, as wide as the hull is there; the keel along its bottom
    for tx in (-12.0, 18.0):
        t = (tx + 50.0) / 100.0
        beam = 50.0 * (math.sin(math.pi * min(1.0, 0.18 + t * 0.92)) ** 0.75) * (0.55 + 0.45 * smooth(0.0, 0.25, t))
        GS.append_box(mesh, opts(), xf((tx, 0.0, 24.0)), 9.0, 2.0 * beam - 4.0, 3.0, 1, 1, 1, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    GS.append_box(mesh, opts(), xf((2.0, 0.0, -24.0)), 84.0, 4.0, 7.0, 1, 1, 1, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    save(mesh, name)


def run(key, fn, *args):
    if want(key):
        try:
            fn(*args)
        except Exception as e:
            import traceback
            unreal.log_error(f"[Wildreach] mesh {args[0] if args else key} failed: {e}\n{traceback.format_exc()}")


if want("basalt"):
    for i, n in enumerate("ABC"):
        basalt("SM_BasaltColumns_" + n, 101 + i)
if want("obsidian"):
    for i, n in enumerate("ABC"):
        obsidian("SM_ObsidianBlade_" + n, 201 + i)
if want("sulfur"):
    sulfur("SM_SulfurCrystal", 301)
if want("serac"):
    for i, n in enumerate("ABCD"):
        run("serac", serac, "SM_Serac_" + n, 401 + i)
if want("rubble"):
    for i, n in enumerate("ABC"):
        rubble("SM_Rubble_" + n, 601 + i)
if want("tent"):
    tent("SM_TentRidge_A", 501, 48.0)
if want("arch"):
    ice_arch("SM_IceArch_A", 701)
if want("pingo"):
    pingo("SM_Pingo_A", 801)
if want("iceblock"):
    for i, n in enumerate("ABC"):
        ice_block("SM_IceBlock_" + n, 1001 + i)
if want("hornito"):
    for i, n in enumerate("AB"):
        hornito("SM_Hornito_" + n, 901 + i)
run("skull", giant_skull, "SM_GiantSkull_A", 1101)
run("jaw", giant_jaw, "SM_GiantJaw_A", 1102)
run("vertebra", giant_vertebra, "SM_GiantVertebra_A", 1103)
run("rib", giant_rib, "SM_GiantRib_A", 1104, 0.0, 1.0)
run("rib", giant_rib, "SM_GiantRib_B", 1105, 0.0, 0.42)
run("rib", giant_rib, "SM_GiantRib_C", 1106, 0.46, 1.0, True)
run("longbone", giant_long_bone, "SM_GiantLongBone_A", 1107)
run("pelvis", giant_pelvis, "SM_GiantPelvis_A", 1108)
run("longboat", longboat, "SM_Longboat_A", 1201)
run("spire", wisp_spire, "SM_WispSpire_A", 1301)
run("spire", wisp_ribs, "SM_WispRibs_A", 1302)
run("glasswall", glasswall, "SM_Glasswall_A", 1401)
run("quarry", quarry_face, "SM_QuarryFace_A", 1501)
run("quarry", cut_block, "SM_CutBlock_A", 1502, 220.0, 170.0, 150.0)
run("quarry", cut_block, "SM_CutBlock_B", 1503, 300.0, 150.0, 120.0)
run("golem", golem_rough, "SM_GolemRough_A", 1504)
for i, n in enumerate("ABCD"):
    run("crag", crag, "SM_Crag_" + n, 1601 + i)
for i, n in enumerate("ABCD"):
    run("cragsoft", crag, "SM_CragSoft_" + n, 1611 + i, True)
for i, n in enumerate("ABCD"):
    run("iceface", ice_face, "SM_IceFace_" + n, 1621 + i)
for i, n in enumerate("ABC"):
    run("pebbles", pebbles, "SM_Pebbles_" + n, 1701 + i)
run("driftwood", driftwood, "SM_Driftwood_A", 1711, 460.0, True, 2)
run("driftwood", driftwood, "SM_Driftwood_B", 1712, 330.0, False, 2)
run("driftwood", driftwood, "SM_Driftwood_C", 1713, 250.0, False, 3)
for i, n in enumerate("ABC"):
    run("wrack", wrack, "SM_Wrack_" + n, 1721 + i)
run("townprops", awning, "SM_Awning_A", 1901)
run("townprops", haystack, "SM_Haystack_A", 1902)
run("townprops", rowboat, "SM_Rowboat_A", 1903)
for i, n in enumerate("AB"):
    run("crops", crop, "SM_WheatClump_" + n, 1801 + i, "wheat")
    run("crops", crop, "SM_BarleyClump_" + n, 1811 + i, "barley")
    run("crops", crop, "SM_Cabbage_" + n, 1821 + i, "cabbage")
