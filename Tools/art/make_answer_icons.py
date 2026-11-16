# The bag icons for flint, flint arrows and levistone moss (wild-answers-1115: Aaron's r-flint and r-moss answers), in
# the bag's own style: a thick dark outline, flat fills, one soft highlight (see stone.png, bloodmoss.png).
#   flint.png           a flint nodule broken open: the chalky cream rind round a glassy grey-black core with
#                       conchoidal ripple arcs and a sharp white glint.
#   flint_arrow.png     the stone arrow's icon with its head knapped from flint (grey-black, a pale edge).
#   levistone_moss.png  a cushion of pale grey-green moss with beard strands hanging under it, their tips glowing cyan.
# Drawn at 4x and reduced (smooth edges). Run: python -I Tools/art/make_answer_icons.py Data/UI/Icons
import math, os, random, sys
from PIL import Image, ImageDraw, ImageFilter

S = 4                 # supersampling
W = 128 * S
INK = (38, 28, 22, 255)


def poly_round(cx, cy, r, n, jitter, seed):
    rnd = random.Random(seed)
    pts = []
    for i in range(n):
        a = 2 * math.pi * i / n + rnd.uniform(-0.12, 0.12)
        rr = r * (1 + rnd.uniform(-jitter, jitter))
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.86))
    return pts


def flint():
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy = 64 * S, 68 * S
    rind = poly_round(cx, cy, 44 * S, 11, 0.16, 7)
    d.polygon(rind, fill=INK)
    inner = [(cx + (x - cx) * 0.9, cy + (y - cy) * 0.9) for x, y in rind]
    d.polygon(inner, fill=(232, 224, 204, 255))                     # the chalky rind
    # the broken face: an inset shell-shaped flake scar showing the core
    core = poly_round(cx + 4 * S, cy - 2 * S, 31 * S, 9, 0.12, 21)
    d.polygon(core, fill=INK)
    core_in = [(cx + 4 * S + (x - cx - 4 * S) * 0.9, cy - 2 * S + (y - cy + 2 * S) * 0.9) for x, y in core]
    d.polygon(core_in, fill=(52, 56, 64, 255))
    # conchoidal ripples: arcs round the point of impact (upper left of the scar)
    px, py = cx - 14 * S, cy - 18 * S
    for k, rr in enumerate((14, 22, 30, 38)):
        col = (92, 98, 110, 255) if k % 2 == 0 else (70, 75, 86, 255)
        d.arc([px - rr * S, py - rr * S, px + rr * S, py + rr * S], 10, 95, fill=col, width=int(2.2 * S))
    # mask the ripples to the core
    mask = Image.new("L", (W, W), 0)
    ImageDraw.Draw(mask).polygon(core_in, fill=255)
    rip = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    rd = ImageDraw.Draw(rip)
    for k, rr in enumerate((14, 22, 30, 38)):
        col = (98, 104, 118, 255) if k % 2 == 0 else (74, 79, 90, 255)
        rd.arc([px - rr * S, py - rr * S, px + rr * S, py + rr * S], 0, 110, fill=col, width=int(2.4 * S))
    img.paste(rip, (0, 0), Image.composite(rip, Image.new("RGBA", (W, W)), mask).getchannel("A"))
    # the glint: a sharp white wedge on the core's upper edge
    d.polygon([(cx - 6 * S, cy - 20 * S), (cx + 10 * S, cy - 24 * S), (cx + 2 * S, cy - 17 * S)], fill=(245, 248, 255, 235))
    # chalk speckles on the rind
    rnd = random.Random(3)
    for _ in range(18):
        a = rnd.uniform(0, 2 * math.pi)
        rr = rnd.uniform(34, 40) * S
        x, y = cx + math.cos(a) * rr, cy + math.sin(a) * rr * 0.86
        d.ellipse([x - 1.6 * S, y - 1.6 * S, x + 1.6 * S, y + 1.6 * S], fill=(200, 190, 168, 255))
    return img.resize((128, 128), Image.LANCZOS)


def flint_arrow(stone_arrow_path):
    src = Image.open(stone_arrow_path).convert("RGBA")
    out = src.copy()
    px = out.load()
    w, h = out.size
    # The head is the pale grey low-saturation point at the top right: knap it from flint (the fletching stays).
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a < 10 or x < w * 0.62 or y > h * 0.42:
                continue
            mx, mn = max(r, g, b), min(r, g, b)
            if mx > 120 and mx - mn < 28:
                t = (mx - 120) / 135.0
                nr = int(46 + 70 * t)
                px[x, y] = (nr, nr + 5, nr + 14, a)
    return out


def moss():
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    rnd = random.Random(11)
    # a chip of levistone keel (the pale blue-grey of the levistone icon) the moss grows on
    slab = [(22, 58), (36, 49), (70, 46), (100, 50), (108, 60), (96, 70), (60, 74), (30, 70)]
    d.polygon([(x * S, y * S) for x, y in slab], fill=INK)
    inner = [(64 + (x - 64) * 0.9, 60 + (y - 60) * 0.8) for x, y in slab]
    d.polygon([(x * S, y * S) for x, y in inner], fill=(196, 210, 222, 255))
    d.line([(34 * S, 66 * S), (64 * S, 69 * S), (92 * S, 64 * S)], fill=(150, 170, 190, 255), width=int(2 * S))
    # beard strands hanging from the slab's underside: thin, uneven, a slight curl, glowing tips
    strands = []
    for i, (x0, L) in enumerate(((36, 20), (46, 34), (55, 26), (63, 42), (71, 30), (80, 38), (90, 22))):
        x0 += rnd.uniform(-2, 2)
        pts = []
        for k in range(15):
            t = k / 14
            pts.append(((x0 + math.sin(t * 4.2 + i * 1.7) * 2.4 * t) * S, (70 + t * L) * S))
        strands.append(pts)
    for pts in strands:
        d.line(pts, fill=INK, width=int(4.6 * S), joint="curve")
    for pts in strands:
        d.line(pts, fill=(168, 186, 150, 255), width=int(2.2 * S), joint="curve")
        x, y = pts[-1]
        d.ellipse([x - 3.4 * S, y - 3.4 * S, x + 3.4 * S, y + 3.4 * S], fill=INK)
        d.ellipse([x - 2.2 * S, y - 2.2 * S, x + 2.2 * S, y + 2.2 * S], fill=(126, 240, 232, 255))
    # the cushion on top: low rounded lumps outlined together
    lumps = [(34, 46, 11), (50, 40, 15), (68, 37, 16), (86, 41, 13), (100, 47, 9), (58, 46, 12), (78, 46, 12)]
    for cx, cy, r in lumps:
        d.ellipse([(cx - r - 3) * S, (cy - r * 0.8 - 3) * S, (cx + r + 3) * S, (cy + r * 0.8 + 3) * S], fill=INK)
    for i, (cx, cy, r) in enumerate(lumps):
        col = (124, 148, 98, 255) if i % 2 == 0 else (106, 132, 84, 255)
        d.ellipse([(cx - r) * S, (cy - r * 0.8) * S, (cx + r) * S, (cy + r * 0.8) * S], fill=col)
    for _ in range(80):
        cx, cy, r = lumps[rnd.randrange(len(lumps))]
        a = rnd.uniform(0, 2 * math.pi)
        rr = rnd.uniform(0, r * 0.8)
        x, y = (cx + math.cos(a) * rr) * S, (cy + math.sin(a) * rr * 0.75) * S
        d.ellipse([x - 1.1 * S, y - 1.1 * S, x + 1.1 * S, y + 1.1 * S], fill=(166, 188, 136, 255))
    for cx, cy, r in lumps[1:4]:
        d.ellipse([(cx - r * 0.55) * S, (cy - r * 0.7) * S, (cx + r * 0.05) * S, (cy - r * 0.3) * S], fill=(196, 214, 168, 210))
    for x, y in ((52, 34), (72, 31), (88, 39), (40, 44)):
        d.ellipse([(x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S], fill=(150, 250, 240, 255))
    return img.resize((128, 128), Image.LANCZOS)


if __name__ == "__main__":
    icons = sys.argv[1] if len(sys.argv) > 1 else "Data/UI/Icons"
    flint().save(os.path.join(icons, "flint.png"))
    flint_arrow(os.path.join(icons, "stone_arrow.png")).save(os.path.join(icons, "flint_arrow.png"))
    moss().save(os.path.join(icons, "levistone_moss.png"))
    print("ok")
