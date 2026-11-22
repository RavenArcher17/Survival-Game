"""Generated meshes for the things the art packs don't have (11-22, Raven: "everything as good as the caves"). Safe to
re-run: each asset is made again in /Game/Art/Gen (in git: our own geometry, no pack's).
Run: UnrealEditor-Cmd Wildreach.uproject -run=pythonscript -script="<full path>/Tools/art/make_meshes.py"
     WR_MESHES=basalt,obsidian (comma list) makes only those.

  SM_BasaltColumns_A..C   clusters of hexagonal basalt columns (Giant's Causeway style), heights stepped, tops a little
                          tilted and broken; about 3 x 3 m across, 5.6 m tall at the highest.
  SM_ObsidianBlade_A..C   clusters of thin faceted glass blades leaning out of a stub, 4.2 m tall at the highest.
  SM_SulfurCrystal        one small crystal: a squashed double point (the sulfur node's crust is covered in them).
  SM_Serac_A..B           tall leaning ice towers with blocky faceted faces.
  SM_Rubble_A..C          a patch of broken basalt about 4.4 m across: ~40 angular stones (5-45 cm), bigger and
                          thicker toward the middle, half sunk; Nanite (the Volcanic's lava_rubble dressing).
  SM_TentRidge_A          a ridge tent's canvas (the wake-up camps): sagging panels, a flared hem tucked into the ground,
                          the back gable closed, a triangular doorway in the front; inside faces of its own. X along the
                          ridge (the door at -X), 280 cm long, 290 across the hem, the ridge 150 cm up. M_BuildTinted
                          (cloth) as an instanced block (WRBuildLook EWRLookMesh::Tent).
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


def save(mesh, name, nanite=False):
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
    rnd = random.Random(seed)
    mesh = unreal.DynamicMesh()
    # A tower of ice broken off a glacier's front: a few fused, tilted blocks, narrowing and leaning, its top snapped off
    # at a slant, finely divided so two scales of noise can split and slump its faces (blocks with a little noise read
    # as stacked boxes). A couple of fallen chunks at its foot.
    z = -60.0
    sx, sy = rnd.uniform(240, 300), rnd.uniform(200, 260)
    for k in range(5):
        h = rnd.uniform(110, 160)
        GS.append_box(mesh, opts(), xf((rnd.uniform(-30, 30), rnd.uniform(-30, 30), z), (rnd.uniform(-7, 7), rnd.uniform(-20, 20), rnd.uniform(-7, 7))), sx, sy, h, 6, 6, 5, unreal.GeometryScriptPrimitiveOriginMode.BASE)
        z += h * 0.88
        sx *= rnd.uniform(0.7, 0.88)
        sy *= rnd.uniform(0.7, 0.88)
    for k in range(rnd.randint(2, 3)):
        a = rnd.uniform(0, 6.28)
        s = rnd.uniform(60, 110)
        GS.append_box(mesh, opts(), xf((math.cos(a) * 190, math.sin(a) * 170, -20), (rnd.uniform(-25, 25), rnd.uniform(0, 360), rnd.uniform(-25, 25))), s, s * 0.8, s * 0.7, 3, 3, 3, unreal.GeometryScriptPrimitiveOriginMode.BASE)
    noise(mesh, 38.0, 0.006, seed)
    noise(mesh, 9.0, 0.03, seed + 7)
    save(mesh, name)


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


if want("basalt"):
    for i, n in enumerate("ABC"):
        basalt("SM_BasaltColumns_" + n, 101 + i)
if want("obsidian"):
    for i, n in enumerate("ABC"):
        obsidian("SM_ObsidianBlade_" + n, 201 + i)
if want("sulfur"):
    sulfur("SM_SulfurCrystal", 301)
if want("serac"):
    for i, n in enumerate("AB"):
        serac("SM_Serac_" + n, 401 + i)
if want("rubble"):
    for i, n in enumerate("ABC"):
        rubble("SM_Rubble_" + n, 601 + i)
if want("tent"):
    tent("SM_TentRidge_A", 501, 48.0)
