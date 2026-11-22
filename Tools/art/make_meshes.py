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
  SM_IceArch_A            a natural ice bridge 72 m across, 13 m high, icicles under it (the Ice Arch); collides.
  SM_Pingo_A              an ice-cored mound 26 m across, 10.5 m high, a cracked crown, one flank broken open.
  SM_IceBlock_A..C        a block of glacier ice 4 m each way, top broken at a slant, faces split (the Crevasse Maze).
  SM_Hornito_A..B         a lava chimney of welded spatter, 20 m tall, an open throat (the Bellows Stacks, hornitos).
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

    def grid(self, ids, out_of):
        """Quads over a grid of point ids (rows x columns, the columns wrapping round); out_of(p) gives each face's
        outward hint from its centre."""
        rows, cols = len(ids), len(ids[0])
        for i in range(rows - 1):
            for j in range(cols):
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
