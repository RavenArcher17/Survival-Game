# The Electric Dreams forest kit: textures capped, meshes measured, Data/forest_art.json written
# (cooks/wild-answers-1116.md 4.1; Aaron 11-16, q-electric-dreams "Yes do the better forest").
#
# Run after Tools/art/electric_dreams_forest.ps1 has copied the kit into Content/ (all six Fab packs present too):
#   UnrealEditor-Cmd.exe Wildreach.uproject -run=pythonscript -script="Tools/art/electric_dreams_forest.py" -unattended -nop4 -nosplash
#
# 1. Every copied texture is capped at 2048 (the scans are 4K-8K; the perf pass's rule for 8 GB cards) and saved.
# 2. Every mesh the forest draws is measured: its height and base (bounds), a tree's trunk (centre and radius from its
#    LOD0 vertices 0.4-1.6 m up), a boulder's capsule along its long axis. Those become the collision the server and
#    every client share (WRWorldDressing.cpp reads them; no mesh is needed for collision, so a PC without the kit still
#    agrees with the server).
# 3. Data/forest_art.json: per forest kind, the meshes that draw it, their measurements, the leaf colour variation.
import json
import math
import os

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
EAL = unreal.EditorAssetLibrary
HB = "/Game/Megascans/3D_Plants/EuropeanHornbeam/Geometry/SimpleWind/SM_EuropeanHornbeam_"
HBM = "/Game/Megascans/3D_Plants/EuropeanHornbeam/Materials/SimpleWind/"
MS = "/Game/Megascans/"


def log(msg):
    unreal.log("EDFOREST " + msg)


def cap_textures(packages):
    capped = 0
    for p in packages:
        a = EAL.load_asset(p)
        if not isinstance(a, unreal.Texture):
            continue
        dirty = False
        if a.get_editor_property("max_texture_size") != 2048:
            a.set_editor_property("max_texture_size", 2048)
            dirty = True
        if dirty:
            EAL.save_loaded_asset(a, only_if_is_dirty=False)
            capped += 1
    log("textures capped at 2048: %d" % capped)


def section_slot(mesh, section):
    for getter in ("StaticMeshEditorSubsystem",):
        try:
            sub = unreal.get_editor_subsystem(getattr(unreal, getter))
            return sub.get_lod_material_slot(mesh, 0, section)
        except Exception:
            pass
    try:
        return unreal.EditorStaticMeshLibrary.get_lod_material_slot(mesh, 0, section)
    except Exception:
        return section


def slot_name(mesh, slot):
    try:
        mats = mesh.get_editor_property("static_materials")
        m = mats[slot].get_editor_property("material_interface")
        return m.get_name() if m else ""
    except Exception:
        return ""


def measure(path, trunk=False, rock=False):
    mesh = EAL.load_asset(path)
    if not mesh:
        log("MISSING " + path)
        return None
    box = mesh.get_bounding_box()
    mn, mx = box.min, box.max
    height = mx.z - mn.z
    row = {"path": path + "." + path.split("/")[-1], "height": round(height, 1), "minZ": round(mn.z, 1),
           "width": round(max(mx.x - mn.x, mx.y - mn.y), 1)}
    colliders = []
    if trunk:
        pts = []
        sections = mesh.get_num_sections(0)
        for s in range(sections):
            name = slot_name(mesh, section_slot(mesh, s)).lower()
            if sections > 1 and "trunk" not in name and "bark" not in name:
                continue
            try:
                verts = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, s)[0]
            except Exception as e:
                log("no vertices for %s section %d: %s" % (path, s, e))
                continue
            for v in verts:
                if mn.z + 40.0 <= v.z <= mn.z + 160.0 and math.hypot(v.x, v.y) < 0.12 * height + 60.0:
                    pts.append((v.x, v.y))
        if len(pts) >= 6:
            cx = sum(p[0] for p in pts) / len(pts)
            cy = sum(p[1] for p in pts) / len(pts)
            d = sorted(math.hypot(p[0] - cx, p[1] - cy) for p in pts)
            r = d[int(len(d) * 0.75)]
        else:
            cx, cy, r = 0.0, 0.0, 0.02 * height
            log("trunk estimated for " + path)
        r = max(8.0, min(r, 0.06 * height))
        top = mn.z + min(450.0, 0.4 * height)
        colliders.append({"a": [round(cx, 1), round(cy, 1), round(mn.z, 1)], "b": [round(cx, 1), round(cy, 1), round(top, 1)], "r": round(r, 1)})
    if rock:
        ex, ey, ez = (mx.x - mn.x) / 2, (mx.y - mn.y) / 2, (mx.z - mn.z) / 2
        cx, cy, cz = (mx.x + mn.x) / 2, (mx.y + mn.y) / 2, mn.z + ez * 0.9
        long_x = ex >= ey
        r = max(20.0, 0.9 * min(ey if long_x else ex, ez))
        half = max(0.0, (ex if long_x else ey) - r)
        ax = [cx - half, cy, cz] if long_x else [cx, cy - half, cz]
        bx = [cx + half, cy, cz] if long_x else [cx, cy + half, cz]
        colliders.append({"a": [round(c, 1) for c in ax], "b": [round(c, 1) for c in bx], "r": round(r, 1)})
    if colliders:
        row["colliders"] = colliders
    return row


def leaf_slot(mesh_path):
    mesh = EAL.load_asset(mesh_path)
    mats = mesh.get_editor_property("static_materials")
    for i, m in enumerate(mats):
        mi = m.get_editor_property("material_interface")
        n = mi.get_name() if mi else ""
        if "TwoSided" in n and "Bush" not in n:
            return i
    return -1


def describe(mi_path):
    mi = EAL.load_asset(mi_path)
    if not mi:
        return "missing"
    out = []
    try:
        for p in mi.get_editor_property("vector_parameter_values"):
            v = p.get_editor_property("parameter_value")
            out.append("%s=(%.2f %.2f %.2f)" % (p.get_editor_property("parameter_info").get_editor_property("name"), v.r, v.g, v.b))
        for p in mi.get_editor_property("scalar_parameter_values"):
            out.append("%s=%.2f" % (p.get_editor_property("parameter_info").get_editor_property("name"), p.get_editor_property("parameter_value")))
    except Exception as e:
        out.append(str(e))
    return " ".join(out)


def main():
    man = json.load(open(os.path.join(ROOT, "Tools", "art", "electric_dreams_forest.json"), encoding="utf-8"))
    cap_textures(man["packages"])

    # The eight hornbeam forest trees, measured, then dealt to the four canopy "species" by crown shape: oak the
    # broadest crowns (spreading), ash and lime the narrowest (tall, light), beech the rest (KCD2's oak-hornbeam wood).
    trees = []
    for i in range(1, 9):
        r = measure(HB + "Forest_%02d" % i, trunk=True)
        if r:
            r["aspect"] = round(r["width"] / max(1.0, r["height"]), 3)
            trees.append(r)
            log("tree %s h %.0f w %.0f aspect %.2f trunk r %s" % (r["path"].split(".")[-1], r["height"], r["width"], r["aspect"], r.get("colliders", [{}])[0].get("r")))
    by_aspect = sorted(trees, key=lambda t: -t["aspect"])
    n = len(by_aspect)
    pick = lambda idx: [dict((k, v) for k, v in by_aspect[j].items() if k != "aspect") for j in idx if 0 <= j < n]
    broad, narrow = list(range(0, 3)), list(range(max(0, n - 3), n))
    middle = [j for j in range(n) if j not in broad[:2] and j not in narrow[-2:]]

    for k in range(6):
        log("leaf colour var %d: %s" % (k, describe(HBM + "MI_EuropeanHornbeam_TwoSided_ColorVar_%02d" % k)))
    var = lambda k: HBM + "MI_EuropeanHornbeam_TwoSided_ColorVar_%02d.MI_EuropeanHornbeam_TwoSided_ColorVar_%02d" % (k, k)

    def kind(meshes, cull, shadow, leaf_var=None, sink=0.0, fit="kit", max_fit=0.0):
        row = {"meshes": meshes, "cull": cull, "shadow": shadow, "sink": sink, "fit": fit}
        if max_fit > 0:
            row["maxFit"] = max_fit
        if leaf_var is not None:
            # Each scan has its own slot order: the leaf card slot is looked up per mesh.
            row["meshes"] = [dict(m) for m in meshes]
            for m in row["meshes"]:
                slot = leaf_slot(m["path"].split(".")[0])
                if slot >= 0:
                    m["materials"] = {str(slot): var(leaf_var)}
        return row

    saplings = [measure(HB + "Sapling_%02d" % i) for i in range(1, 7)]
    # Whole fern plants only (the lady fern's lying fronds and the beech fern's single leaves are pieces, not plants).
    ferns = [measure(MS + "3D_Plants/SilverLadyFern/SM_SilverLadyFern_%02d" % i) for i in range(1, 4)] + \
            [measure(MS + "3D_Plants/Fern/SM_Fern_%02d" % i) for i in range(1, 7)]
    elder = [measure(MS + "3D_Plants/Elderberry/SM_Elderberry_%02d" % i) for i in (1, 2, 3, 4, 5, 6, 10, 11, 12, 13)]
    rocks = [measure(MS + "3D_Assets/MossyForestBoulder/SM_MossyForestBoulder_%02d" % i, rock=True) for i in (1, 2)] + \
            [measure(MS + "3D_Assets/LichenedForestBoulder/SM_LichenedForestBoulder_01", rock=True)]
    # The three leaf-litter patches (0.8-1.7 m); the rest of the set are single leaves.
    leaves = [measure(MS + "3D_Plants/DeadLeaves/SM_DeadLeaves_%02d" % i) for i in (1, 12, 13)]
    cover = [measure(MS + "3D_Plants/GroundCover/SM_GroundCover_%02d" % i) for i in range(1, 9)]
    moss = [measure(MS + "3D_Plants/CustomMoss/SM_CustomMoss_%02d" % i) for i in range(1, 6)]
    ok = lambda l: [m for m in l if m]
    for name, l in (("sapling", saplings), ("fern", ferns), ("elder", elder), ("rock", rocks), ("leaves", leaves), ("cover", cover), ("moss", moss)):
        for m in ok(l):
            log("%s %s h %.0f w %.0f" % (name, m["path"].split(".")[-1], m["height"], m["width"]))

    # Fit caps: the hornbeams are 11-16 m scans; the generator asks 18-38 m of the canopy. Real hornbeams reach 25-30 m, so a
    # tree is scaled to its height up to 1.75x (leaves stay leaf-sized) and the canopy stands a little lower than the kit's.
    kinds = {
        # Leaf colour per "species" from the hornbeam's own colour variations (read from the material instances): oak the
        # deep green (var 5: albedo tint 0.14 / 0.35 / 0.26), beech the bright yellow-green (var 4: tint 1 / 1 / 0.61), lime
        # the warm default (var 0), ash the plain one (var 3); saplings in the gaps' sun get the season overlay (var 2).
        # Var 1 (a red-brown tint) is autumn and left out: the forest is in leaf.
        "forest_oak": kind(pick(broad), 26000, True, 5, 0.01, "kit", 1.75),
        "forest_beech": kind(pick(middle), 26000, True, 4, 0.01, "kit", 1.75),
        "forest_lime": kind(pick(narrow[:2] + middle[:1]), 26000, True, 0, 0.01, "kit", 1.75),
        "forest_ash": kind(pick(narrow), 26000, True, 3, 0.01, "kit", 1.75),
        "forest_sapling": kind(ok(saplings), 10000, False, 2, 0.0, "kit", 2.2),
        "forest_fern": kind(ok(ferns), 4000, False, None, 0.0, "kit", 1.8),
        "forest_rock": kind(ok(rocks), 16000, True, None, 0.15, "kit", 3.5),
        # The ground layer (forest_dressing.mjs writes Data/forest_ground.json): native size x the row's scale, laid on the
        # ground's slope; looks only.
        "forest_leaves": kind(ok(leaves), 3500, False, None, 0.0, "native"),
        # Wood anemone, herbs and young elder in the light: low plants at the gaps' rims and the meadows' edges.
        "forest_cover": kind(ok(cover) + ok(elder), 3500, False, None, 0.0, "native"),
        "forest_moss": kind(ok(moss), 3000, False, None, 0.0, "native"),
    }
    out = {
        "_comment": "Generated by Tools/art/electric_dreams_forest.py (edit the script, not this file; cooks/wild-answers-1116.md 4.1). "
                    "The Ancient forest's kinds drawn with Epic's Electric Dreams scans (Tools/art/electric_dreams_forest.ps1 copies them in). "
                    "Per kind: meshes {path, height, minZ (cm, the mesh's own), colliders [{a, b, r} capsules in mesh space]}, cull (cm, x world.decorationDistanceScale), "
                    "shadow, sink (a share of the height pressed into the ground: Electric Dreams' embed), fit ('kit': scaled to the height forest_dressing.mjs gave the instance, at most maxFit x the mesh; "
                    "'native': the mesh's own size x the instance's scale), a mesh's materials {slot: material} (the hornbeams' leaf colour variation per canopy species). "
                    "Instance i draws mesh i % N. Without the kit a kind draws its tree-kit look, collision unchanged.",
        "kinds": kinds,
    }
    path = os.path.join(ROOT, "Data", "forest_art.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
    log("wrote %s: %s" % (path, ", ".join("%s %d" % (k, len(v["meshes"])) for k, v in kinds.items())))


main()
