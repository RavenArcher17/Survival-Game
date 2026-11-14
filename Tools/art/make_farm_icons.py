"""Item icons for farming and food (Data/UI/Icons/*.png, 128 px; cooks/build-farming.md), in the UI's flat style like
make_fish_icons.py: a thick dark outline, flat fills, one pale highlight, drawn at 4x and scaled down. Every crop has
its seeds (a tied pouch banded in the crop's colour) and its produce in its own shape; dishes are bowls, plates,
loaves and pies in what's in them; preserved food, kitchen materials, the biome lures, the new fish and the legends'
trophies. Run: python Tools/art/make_farm_icons.py  (only writes icons that are missing; --all redraws them)."""
import math, os, sys, json
from PIL import Image, ImageDraw, ImageFilter

import types
# The fish style's helpers (fish(), canvas(), save(), poly(), rot(), INK, HI, S, W) from make_fish_icons.py: only its
# definitions, not the calls at its end that draw the volcanic and tundra fish.
_src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'make_fish_icons.py'), encoding='utf-8').read().splitlines()
_cut = next(i for i, l in enumerate(_src) if l.startswith('fish(') or l.startswith('rod('))
F = types.ModuleType('fish_style')
F.__dict__['__file__'] = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'make_fish_icons.py')
exec(chr(10).join(_src[:_cut]), F.__dict__)

ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
OUT = os.path.join(ROOT, 'Data', 'UI', 'Icons')
S, W, INK, HI = F.S, F.W, F.INK, F.HI
ALL = '--all' in sys.argv
LW = 5 * S


def rgb(c, a=255):
    return (int(c[0]), int(c[1]), int(c[2]), a)


def shade(c, k):
    return rgb([max(0, min(255, v * k)) for v in c[:3]])


def canvas():
    return F.canvas()


def done(im, name):
    if not ALL and os.path.exists(os.path.join(OUT, name + '.png')):
        return
    F.save(im, name)


def ell(d, cx, cy, rx, ry, fill, outline=INK, width=LW):
    d.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), fill=fill, outline=outline, width=width)


def hi_arc(d, cx, cy, rx, ry, a0=200, a1=250):
    d.arc((cx - rx * 0.7, cy - ry * 0.7, cx + rx * 0.7, cy + ry * 0.7), a0, a1, fill=HI, width=4 * S)


def P(x, y):
    return (x * S, y * S)


def poly(d, pts, fill, width=LW):
    F.poly(d, [P(*p) for p in pts], fill, INK, width)


def leaf(d, x, y, ang, length, width, col):
    a = math.radians(ang)
    dx, dy = math.cos(a), math.sin(a)
    nx, ny = -dy, dx
    pts = []
    for i in range(9):
        t = i / 8
        w = math.sin(math.pi * t) * width
        pts.append((x + dx * length * t + nx * w, y + dy * length * t + ny * w))
    for i in range(8, -1, -1):
        t = i / 8
        w = math.sin(math.pi * t) * width
        pts.append((x + dx * length * t - nx * w, y + dy * length * t - ny * w))
    poly(d, pts, rgb(col))
    d.line([P(x, y), P(x + dx * length * 0.85, y + dy * length * 0.85)], fill=shade(col, 0.6), width=2 * S)


# ---------------------------------------------------------------- seeds: a tied cloth pouch banded in the crop's colour

def seeds(name, col, seed_col):
    im, d = canvas()
    body = [(34, 104), (30, 70), (44, 50), (52, 44), (76, 44), (84, 50), (98, 70), (94, 104), (64, 110)]
    poly(d, body, rgb((196, 168, 118)))
    poly(d, [(46, 50), (40, 36), (52, 40), (64, 32), (76, 40), (88, 36), (82, 50)], rgb((182, 152, 104)))
    d.line([P(44, 50), P(84, 50)], fill=INK, width=4 * S)
    d.rectangle((38 * S, 72 * S, 92 * S, 86 * S), fill=rgb(col))
    d.line([P(36, 72), P(92, 72)], fill=INK, width=3 * S)
    d.line([P(36, 86), P(93, 86)], fill=INK, width=3 * S)
    for i, (x, y) in enumerate([(98, 104), (108, 96), (112, 108)]):
        ell(d, x * S, y * S, 5 * S, 3.5 * S, rgb(seed_col), width=3 * S)
    d.line([P(40, 64), P(46, 56)], fill=HI, width=4 * S)
    done(im, name)


# ---------------------------------------------------------------- produce shapes

def pepper(name, col):
    im, d = canvas()
    pts = [(40, 40), (58, 36), (74, 46), (88, 66), (98, 94), (100, 112), (88, 98), (70, 76), (50, 60)]
    poly(d, pts, rgb(col))
    poly(d, [(36, 30), (48, 26), (58, 36), (40, 42)], rgb((70, 110, 40)))
    d.line([P(44, 28), P(34, 16)], fill=INK, width=6 * S)
    d.line([P(56, 46), P(78, 64)], fill=HI, width=4 * S)
    done(im, name)


def grapes(name, col, leafc=(70, 110, 45)):
    im, d = canvas()
    leaf(d, 70, 30, -20, 40, 14, leafc)
    d.line([P(60, 26), P(62, 42)], fill=INK, width=5 * S)
    for (x, y) in [(50, 50), (66, 50), (82, 50), (42, 64), (58, 64), (74, 64), (90, 64), (50, 78), (66, 78), (82, 78), (58, 92), (74, 92), (66, 106)]:
        ell(d, x * S, y * S, 9 * S, 9 * S, rgb(col), width=4 * S)
        d.arc(((x - 6) * S, (y - 6) * S, (x + 2) * S, (y + 2) * S), 190, 260, fill=HI, width=3 * S)
    done(im, name)


def root(name, top, bottom, leafc, fork=False):
    im, d = canvas()
    for a, l in [(-110, 40), (-80, 46), (-55, 38)]:
        leaf(d, 64, 50, a, l, 9, leafc)
    if fork:
        poly(d, [(46, 52), (82, 52), (86, 72), (76, 92), (84, 116), (70, 104), (64, 92), (56, 104), (42, 116), (50, 92), (42, 72)], rgb(bottom))
        poly(d, [(46, 52), (82, 52), (84, 62), (44, 62)], rgb(top))
    else:
        pts = [(64 + 30 * math.cos(math.radians(a)), 76 + 28 * math.sin(math.radians(a))) for a in range(-180, 181, 20)]
        pts += [(64, 118)]
        poly(d, pts[:-1] + [(70, 104), (64, 118), (58, 104)], rgb(bottom))
        d.pieslice((34 * S, 48 * S, 94 * S, 104 * S), 180, 360, fill=rgb(top), outline=INK, width=LW)
    d.line([P(48, 70), P(56, 62)], fill=HI, width=4 * S)
    done(im, name)


def sheaf(name, ear, stalk):
    im, d = canvas()
    for i, ang in enumerate([-100, -88, -76, -64]):
        a = math.radians(ang)
        x0, y0 = 64, 110
        x1, y1 = x0 + math.cos(a) * 80, y0 + math.sin(a) * 80
        d.line([P(x0, y0), P(x1, y1)], fill=INK, width=7 * S)
        d.line([P(x0, y0), P(x1, y1)], fill=rgb(stalk), width=3 * S)
        ell(d, (x1 + math.cos(a) * 8) * S, (y1 + math.sin(a) * 8) * S, 7 * S, 13 * S, rgb(ear), width=4 * S)
    d.rectangle((54 * S, 86 * S, 74 * S, 94 * S), fill=rgb((150, 110, 60)), outline=INK, width=3 * S)
    done(im, name)


def fruit(name, col, kind='apple', leafc=(70, 120, 45)):
    im, d = canvas()
    if kind == 'pear':
        pts = [(64 + 26 * math.cos(math.radians(a)), 74 + 34 * math.sin(math.radians(a))) for a in range(0, 360, 15)]
        poly(d, pts, rgb(col))
        for (x, y) in [(52, 62), (72, 58), (60, 82), (80, 78), (48, 90), (70, 96)]:
            d.line([P(x, y), P(x + 3, y - 3)], fill=rgb((240, 230, 160)), width=3 * S)
        poly(d, [(56, 40), (72, 40), (70, 46), (58, 46)], rgb((70, 110, 70)))
    elif kind == 'apple':
        ell(d, 64 * S, 76 * S, 34 * S, 32 * S, rgb(col))
        d.line([P(64, 46), P(68, 28)], fill=INK, width=6 * S)
        leaf(d, 68, 34, -30, 26, 9, leafc)
        hi_arc(d, 64 * S, 76 * S, 34 * S, 32 * S)
    elif kind == 'berry':
        for (x, y) in [(54, 66), (70, 62), (62, 78), (78, 78), (48, 82), (64, 92), (78, 94)]:
            ell(d, x * S, y * S, 9 * S, 9 * S, rgb(col), width=4 * S)
        leaf(d, 64, 54, -95, 26, 10, leafc)
    elif kind == 'melon':
        ell(d, 64 * S, 72 * S, 42 * S, 34 * S, rgb(col))
        for k in (-24, -8, 8, 24):
            d.arc(((64 + k - 12) * S, 40 * S, (64 + k + 12) * S, 104 * S), 270, 450 if k < 0 else 450, fill=shade(col, 0.7), width=3 * S)
        glow = Image.new('RGBA', (W, W), (0, 0, 0, 0))
        ImageDraw.Draw(glow).ellipse((14 * S, 30 * S, 114 * S, 114 * S), fill=rgb(col, 90))
        im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(10 * S)), (0, 0))
        d = ImageDraw.Draw(im)
        ell(d, 64 * S, 72 * S, 42 * S, 34 * S, None)
        hi_arc(d, 64 * S, 72 * S, 42 * S, 34 * S)
    elif kind == 'dates':
        for (x, y, a) in [(50, 70, 20), (64, 64, -10), (78, 72, 15), (56, 88, -20), (72, 90, 10)]:
            pts = [(x + 8 * math.cos(math.radians(t)) * math.cos(math.radians(a)) - 15 * math.sin(math.radians(t)) * math.sin(math.radians(a)),
                    y + 8 * math.cos(math.radians(t)) * math.sin(math.radians(a)) + 15 * math.sin(math.radians(t)) * math.cos(math.radians(a))) for t in range(0, 360, 30)]
            poly(d, pts, rgb(col), 4 * S)
        d.line([P(64, 48), P(64, 30)], fill=INK, width=5 * S)
    elif kind == 'beads':
        for k in range(3):
            x0 = 44 + k * 20
            d.line([P(x0, 30), P(x0 + 6, 110)], fill=INK, width=5 * S)
            for j in range(6):
                ell(d, (x0 + j) * S, (36 + j * 14) * S, 7 * S, 7 * S, rgb(col), width=3 * S)
    done(im, name)


def stalks(name, col, leafc):
    im, d = canvas()
    for k, x in enumerate([46, 62, 78]):
        poly(d, [(x - 6, 116), (x - 4, 44), (x + 4, 44), (x + 6, 116)], rgb(col))
        leaf(d, x, 46, -100 + k * 25, 24, 12, leafc)
    done(im, name)


def kelp(name, col):
    im, d = canvas()
    for k, x0 in enumerate([42, 64, 86]):
        pts = []
        for i in range(12):
            y = 118 - i * 8
            pts.append((x0 + math.sin(i * 0.9 + k) * 8 - 6, y))
        back = [(x + 12, y) for x, y in pts[::-1]]
        poly(d, pts + back, rgb(col), 4 * S)
    done(im, name)


def mushroom(name, cap, stem, kind='cap'):
    im, d = canvas()
    if kind == 'mane':
        ell(d, 64 * S, 70 * S, 34 * S, 30 * S, rgb(cap))
        for i in range(14):
            x = 38 + i * 4
            d.line([P(x, 78), P(x, 98)], fill=shade(cap, 0.75), width=3 * S)
    elif kind == 'truffle':
        pts = [(64 + 30 * math.cos(math.radians(a)) * (1 + 0.08 * math.sin(a * 0.3)), 74 + 26 * math.sin(math.radians(a))) for a in range(0, 360, 20)]
        poly(d, pts, rgb(cap))
        for (x, y) in [(52, 66), (70, 62), (60, 82), (78, 80)]:
            d.ellipse(((x - 3) * S, (y - 3) * S, (x + 3) * S, (y + 3) * S), fill=shade(cap, 0.6))
        glow = Image.new('RGBA', (W, W), (0, 0, 0, 0))
        ImageDraw.Draw(glow).ellipse((40 * S, 56 * S, 88 * S, 92 * S), fill=(255, 120, 40, 70))
        im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(8 * S)))
    else:
        for (x, y, r) in [(50, 62, 22), (78, 76, 18), (58, 88, 16)]:
            poly(d, [(x - 4, y), (x - 6, y + 26), (x + 6, y + 26), (x + 4, y)], rgb(stem), 4 * S)
            d.chord(((x - r) * S, (y - r * 0.7) * S, (x + r) * S, (y + r * 0.7) * S), 180, 360, fill=rgb(cap), outline=INK, width=LW)
    done(im, name)


def flower_root(name, petal, rootc, leafc):
    im, d = canvas()
    d.line([P(64, 80), P(64, 30)], fill=INK, width=6 * S)
    d.line([P(64, 80), P(64, 30)], fill=rgb(leafc), width=2 * S)
    for a in range(0, 360, 72):
        ra = math.radians(a)
        cx, cy = 64 + math.cos(ra) * 12, 30 + math.sin(ra) * 12
        ell(d, cx * S, cy * S, 9 * S, 9 * S, rgb(petal), width=3 * S)
    ell(d, 64 * S, 30 * S, 5 * S, 5 * S, rgb((240, 200, 60)), width=3 * S)
    leaf(d, 64, 64, -150, 22, 7, leafc)
    leaf(d, 64, 58, -30, 22, 7, leafc)
    poly(d, [(54, 80), (74, 80), (78, 96), (70, 116), (64, 104), (56, 118), (50, 96)], rgb(rootc))
    done(im, name)


# ---------------------------------------------------------------- dishes

def bowl(name, contents, bits=None, steam=True):
    im, d = canvas()
    if steam:
        for x in (50, 64, 78):
            d.arc(((x - 8) * S, 18 * S, (x + 8) * S, 44 * S), 100, 260, fill=(240, 240, 240, 150), width=3 * S)
    ell(d, 64 * S, 62 * S, 46 * S, 14 * S, rgb(contents))
    d.chord((18 * S, 34 * S, 110 * S, 112 * S), 0, 180, fill=rgb((150, 100, 60)), outline=INK, width=LW)
    d.line([P(28, 76), P(44, 90)], fill=HI, width=4 * S)
    for (x, y, c) in (bits or []):
        ell(d, x * S, y * S, 6 * S, 4 * S, rgb(c), width=3 * S)
    done(im, name)


def loaf(name, col, score=(240, 220, 170), flat=False):
    im, d = canvas()
    if flat:
        ell(d, 64 * S, 74 * S, 46 * S, 22 * S, rgb(col))
        for (x, y) in [(46, 70), (62, 66), (78, 72), (56, 80), (72, 82)]:
            d.ellipse(((x - 3) * S, (y - 2) * S, (x + 3) * S, (y + 2) * S), fill=shade(col, 0.55))
    else:
        d.chord((16 * S, 40 * S, 112 * S, 120 * S), 180, 360, fill=rgb(col), outline=INK, width=LW)
        d.rectangle((16 * S, 78 * S, 112 * S, 92 * S), fill=shade(col, 0.85), outline=INK, width=LW)
        for x in (40, 60, 80):
            d.line([P(x, 56), P(x + 10, 68)], fill=rgb(score), width=4 * S)
    done(im, name)


def pie(name, filling, cake=False):
    im, d = canvas()
    if cake:
        d.rectangle((24 * S, 58 * S, 104 * S, 100 * S), fill=rgb((214, 168, 92)), outline=INK, width=LW)
        ell(d, 64 * S, 58 * S, 40 * S, 12 * S, rgb(filling))
        d.line([P(24, 78), P(104, 78)], fill=rgb((240, 210, 120)), width=4 * S)
    else:
        ell(d, 64 * S, 78 * S, 46 * S, 24 * S, rgb((200, 150, 80)))
        ell(d, 64 * S, 74 * S, 38 * S, 18 * S, rgb(filling))
        for k in (-20, 0, 20):
            d.line([P(64 + k - 14, 62), P(64 + k + 14, 88)], fill=rgb((220, 175, 100)), width=5 * S)
            d.line([P(64 + k + 14, 62), P(64 + k - 14, 88)], fill=rgb((220, 175, 100)), width=5 * S)
        ell(d, 64 * S, 74 * S, 38 * S, 18 * S, None)
    done(im, name)


def plate(name, items):
    im, d = canvas()
    ell(d, 64 * S, 80 * S, 50 * S, 22 * S, rgb((200, 196, 186)))
    for f in items:
        f(d)
    done(im, name)


def feast(name, n=6):
    im, d = canvas()
    d.rectangle((14 * S, 84 * S, 114 * S, 100 * S), fill=rgb((150, 100, 60)), outline=INK, width=LW)
    ell(d, 54 * S, 70 * S, 30 * S, 18 * S, rgb((150, 80, 40)))
    d.line([P(70, 60), P(92, 44)], fill=INK, width=7 * S)
    d.line([P(70, 60), P(92, 44)], fill=rgb((240, 230, 210)), width=3 * S)
    for k in range(min(n, 4)):
        ell(d, (88 + (k % 2) * 10) * S, (76 - (k // 2) * 8) * S, 7 * S, 6 * S, rgb((120, 160, 60) if k % 2 else (200, 60, 40)), width=3 * S)
    hi_arc(d, 54 * S, 70 * S, 30 * S, 18 * S)
    done(im, name)


def slab(name, col, crystals=None, string=False, strips=False):
    im, d = canvas()
    if strips:
        for k in range(3):
            poly(d, [(30 + k * 22, 32), (44 + k * 22, 30), (50 + k * 22, 110), (36 + k * 22, 112)], rgb(shade(col, 1 - k * 0.08)), 4 * S)
    else:
        poly(d, [(26, 58), (60, 40), (102, 52), (104, 84), (70, 104), (28, 92)], rgb(col))
        d.line([P(40, 70), P(88, 62)], fill=rgb((235, 220, 210)), width=5 * S)
    if crystals:
        for (x, y) in [(44, 56), (62, 50), (82, 58), (52, 78), (76, 80), (92, 70)]:
            poly(d, [(x, y - 4), (x + 4, y), (x, y + 4), (x - 4, y)], rgb(crystals), 2 * S)
    if string:
        d.line([P(40, 46), P(54, 18)], fill=INK, width=4 * S)
        d.line([P(54, 18), P(70, 14)], fill=INK, width=4 * S)
    done(im, name)


def pile(name, col, n=9, r=8):
    im, d = canvas()
    pos = [(44, 92), (60, 94), (76, 92), (90, 96), (52, 80), (68, 78), (82, 82), (60, 66), (74, 66)]
    for (x, y) in pos[:n]:
        pts = [(x + r * math.cos(math.radians(a)) * (1 + 0.15 * math.sin(a)), y + r * 0.8 * math.sin(math.radians(a))) for a in range(0, 360, 40)]
        poly(d, pts, rgb(col), 3 * S)
    done(im, name)


# ---------------------------------------------------------------- materials

def heap(name, col, flies=False, crystals=False):
    im, d = canvas()
    d.chord((16 * S, 50 * S, 112 * S, 140 * S), 180, 360, fill=rgb(col), outline=INK, width=LW)
    d.line([P(16, 95), P(112, 95)], fill=INK, width=LW)
    for (x, y) in [(40, 80), (60, 70), (80, 78), (52, 88), (90, 88)]:
        if crystals:
            poly(d, [(x, y - 5), (x + 5, y), (x, y + 5), (x - 5, y)], rgb((255, 255, 255)), 2 * S)
        else:
            d.ellipse(((x - 3) * S, (y - 2) * S, (x + 3) * S, (y + 2) * S), fill=shade(col, 0.6))
    if flies:
        for (x, y) in [(40, 40), (74, 30), (92, 46)]:
            d.ellipse(((x - 3) * S, (y - 3) * S, (x + 3) * S, (y + 3) * S), fill=INK)
            d.line([P(x - 6, y - 4), P(x, y)], fill=(255, 255, 255, 160), width=2 * S)
    d.line([P(40, 70), P(54, 62)], fill=HI, width=4 * S)
    done(im, name)


def jar(name, col, lid=(150, 100, 60), small=False):
    im, d = canvas()
    s = 0.75 if small else 1.0
    x0, x1 = 64 - 28 * s, 64 + 28 * s
    d.rounded_rectangle((x0 * S, (48 + (1 - s) * 30) * S, x1 * S, 112 * S), radius=10 * S, fill=rgb(col), outline=INK, width=LW)
    d.rectangle(((64 - 22 * s) * S, (38 + (1 - s) * 30) * S, (64 + 22 * s) * S, (50 + (1 - s) * 30) * S), fill=rgb(lid), outline=INK, width=4 * S)
    d.line([P(x0 + 8, 64 + (1 - s) * 30), P(x0 + 8, 96)], fill=HI, width=4 * S)
    done(im, name)


def block(name, col):
    im, d = canvas()
    poly(d, [(28, 50), (64, 34), (100, 50), (64, 66)], rgb(shade(col, 1.15)))
    poly(d, [(28, 50), (64, 66), (64, 110), (28, 94)], rgb(col))
    poly(d, [(64, 66), (100, 50), (100, 94), (64, 110)], rgb(shade(col, 0.8)))
    d.line([P(36, 60), P(36, 84)], fill=HI, width=4 * S)
    done(im, name)


def feather(name, col, fluffy=False):
    im, d = canvas()
    if fluffy:
        for (x, y, r) in [(54, 70, 22), (74, 66, 20), (64, 84, 22), (82, 84, 16), (46, 86, 16)]:
            ell(d, x * S, y * S, r * S, r * 0.85 * S, rgb(col), width=3 * S)
    else:
        pts = []
        for i in range(10):
            t = i / 9
            pts.append((30 + t * 70 - math.sin(math.pi * t) * 16, 106 - t * 78 - math.sin(math.pi * t) * 14))
        back = [(x + math.sin(math.pi * i / 9) * 22, y + math.sin(math.pi * i / 9) * 10) for i, (x, y) in enumerate(pts)][::-1]
        poly(d, pts + back, rgb(col))
        d.line([P(26, 112), P(98, 28)], fill=INK, width=4 * S)
    done(im, name)


def scrap_paper(name):
    im, d = canvas()
    poly(d, [(26, 30), (96, 24), (104, 60), (94, 70), (100, 104), (34, 108), (30, 80), (22, 66)], rgb((226, 206, 160)))
    for y in (44, 56, 68, 80, 92):
        d.line([P(36, y), P(86 - (y % 3) * 6, y - 2)], fill=rgb((120, 90, 60)), width=3 * S)
    ell(d, 84 * S, 90 * S, 8 * S, 8 * S, rgb((150, 60, 40)), width=3 * S)
    done(im, name)


def lure(name, col, accent):
    im, d = canvas()
    d.arc((60 * S, 80 * S, 96 * S, 116 * S), 0, 200, fill=INK, width=6 * S)
    d.line([P(96, 98), P(96, 74)], fill=INK, width=6 * S)
    d.line([P(96, 98), P(96, 74)], fill=rgb((200, 200, 210)), width=2 * S)
    pts = [(64 + 30 * math.cos(math.radians(a)), 52 + 14 * math.sin(math.radians(a))) for a in range(0, 360, 20)]
    F.poly(d, F.rot([P(*p) for p in pts], -0.5, 64 * S, 52 * S), rgb(col), INK, LW)
    ell(d, 46 * S, 62 * S, 4 * S, 4 * S, rgb(accent), width=2 * S)
    for k in range(3):
        d.line([P(78 + k * 4, 40), P(90 + k * 4, 24)], fill=rgb(accent), width=3 * S)
    d.line([P(40, 40), P(28, 24)], fill=INK, width=3 * S)
    done(im, name)


def plugs(name, col):
    im, d = canvas()
    for (x, y) in [(48, 64), (80, 72)]:
        d.rounded_rectangle(((x - 12) * S, (y - 18) * S, (x + 12) * S, (y + 18) * S), radius=12 * S, fill=rgb(col), outline=INK, width=LW)
        d.line([P(x - 5, y - 10), P(x - 5, y + 6)], fill=HI, width=3 * S)
    done(im, name)


def bones(name):
    im, d = canvas()
    d.line([P(28, 80), P(96, 64)], fill=INK, width=9 * S)
    d.line([P(28, 80), P(96, 64)], fill=rgb((230, 222, 200)), width=4 * S)
    for k in range(5):
        x = 40 + k * 12
        d.line([P(x, 78 - k * 3), P(x - 4, 60 - k * 3)], fill=INK, width=4 * S)
        d.line([P(x, 78 - k * 3), P(x + 4, 96 - k * 3)], fill=INK, width=4 * S)
    poly(d, [(92, 54), (112, 60), (110, 76), (94, 76)], rgb((150, 160, 160)))
    poly(d, [(28, 80), (14, 68), (14, 92)], rgb((150, 160, 160)), 4 * S)
    done(im, name)


def golden(name):
    im, d = canvas()
    glow = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((20 * S, 30 * S, 108 * S, 118 * S), fill=(255, 210, 80, 110))
    im.alpha_composite(glow.filter(ImageFilter.GaussianBlur(12 * S)))
    d = ImageDraw.Draw(im)
    for a, l in [(-110, 34), (-80, 40), (-55, 32)]:
        leaf(d, 64, 54, a, l, 8, (110, 140, 50))
    ell(d, 64 * S, 80 * S, 30 * S, 28 * S, rgb((236, 186, 60)))
    hi_arc(d, 64 * S, 80 * S, 30 * S, 28 * S)
    for (x, y) in [(96, 46), (30, 56), (100, 96)]:
        poly(d, [(x, y - 8), (x + 2, y - 2), (x + 8, y), (x + 2, y + 2), (x, y + 8), (x - 2, y + 2), (x - 8, y), (x - 2, y - 2)], rgb((255, 240, 180)), 2 * S)
    done(im, name)


def trophy(name, back, belly, fins):
    im, d = canvas()
    d.rounded_rectangle((10 * S, 36 * S, 118 * S, 98 * S), radius=10 * S, fill=rgb((120, 78, 42)), outline=INK, width=LW)
    for y in (56, 76):
        d.line([P(14, y), P(114, y)], fill=rgb((96, 60, 30)), width=3 * S)
    sub = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    # A fish drawn by make_fish_icons, scaled onto the board.
    tmp = os.path.join(OUT, '_tmp_trophy_fish')
    old = F.OUT
    F.OUT = os.path.dirname(tmp)
    F.fish(os.path.basename(tmp), rgb(back), rgb(belly), rgb(fins), length=330, depth=110, angle=0.0)
    F.OUT = old
    fish_im = Image.open(tmp + '.png').convert('RGBA').resize((int(W * 0.9), int(W * 0.9)), Image.LANCZOS)
    os.remove(tmp + '.png')
    im.alpha_composite(fish_im, (int(W * 0.05), int(W * 0.02)))
    d = ImageDraw.Draw(im)
    for x in (42, 86):
        ell(d, x * S, 98 * S, 4 * S, 4 * S, rgb((60, 60, 60)), width=2 * S)
    done(im, name)


# ---------------------------------------------------------------- what to draw

def main():
    os.makedirs(OUT, exist_ok=True)
    farming = json.load(open(os.path.join(ROOT, 'Data', 'farming.json'), encoding='utf-8'))
    # Seeds: a pouch banded in the crop's fruit colour, its seeds its stem colour.
    for c in farming['crops']:
        if c['id'] == 'berry':
            continue
        seeds(c['seed'], c['fruit'] if c['id'] not in ('sea_kelp', 'stormrye', 'black_rye', 'ashen_grain') else c['leaf'], [min(255, v * 0.8) for v in c['stem']])
    col = {c['id']: c for c in farming['crops']}
    pepper('fire_pepper', col['fire_pepper']['fruit'])
    grapes('ember_grape', col['ember_grape']['fruit'])
    root('snow_turnip', (170, 110, 190), (236, 230, 240), (70, 110, 60))
    flower_root('roseroot', (232, 200, 60), (210, 130, 120), (80, 120, 70))
    sheaf('stormrye', (196, 170, 96), (150, 150, 110))
    flower_root('gentian', (40, 70, 190), (170, 120, 60), (70, 110, 70))
    fruit('prickly_pear', (190, 30, 110), 'pear')
    fruit('dates', (120, 60, 25), 'dates')
    sheaf('meadow_oats', (220, 200, 140), (170, 170, 100))
    fruit('idunn_apple', (210, 120, 30), 'apple')
    sheaf('windflax', (110, 150, 230), (90, 130, 80))
    fruit('cloudberry', (232, 150, 40), 'berry')
    stalks('candle_rhubarb', (238, 160, 176), (200, 210, 120))
    fruit('glowmelon', (110, 220, 190), 'melon')
    sheaf('bog_rice', (220, 210, 160), (100, 130, 70))
    flower_root('bloodroot', (245, 245, 240), (160, 20, 20), (70, 110, 60))
    kelp('sea_kelp', (110, 96, 40))
    fruit('sea_grapes', (110, 190, 80), 'beads')
    sheaf('black_rye', (70, 60, 55), (90, 90, 80))
    flower_root('asphodel', (232, 228, 218), (180, 170, 140), (90, 110, 80))
    mushroom('oyster_cap', (190, 180, 160), (210, 200, 180))
    mushroom('lions_mane', (245, 240, 230), (230, 225, 215), 'mane')
    sheaf('ashen_grain', (160, 160, 155), (130, 130, 125))
    root('mandrake', (60, 80, 50), (150, 115, 85), (40, 60, 40), fork=True)

    golden('golden_crop')
    heap('compost', (70, 48, 30))
    heap('spoiled_food', (110, 120, 80), flies=True)
    heap('salt', (236, 236, 232), crystals=True)
    block('ice_block', (170, 210, 236))
    jar('honey', (226, 150, 30))
    block('hiveback_wax', (226, 196, 100))
    jar('royal_jelly', (246, 236, 200), small=True)
    plugs('wax_earplugs', (230, 200, 110))
    heap('animal_fat', (236, 222, 180))
    feather('feathers', (236, 236, 240))
    feather('eiderdown', (250, 248, 244), fluffy=True)
    mushroom('ember_truffle', (90, 60, 40), (90, 60, 40), 'truffle')
    bones('fish_scraps')
    scrap_paper('recipe_scrap')
    bowl('grey_mush', (150, 150, 140), steam=False)
    plate('roast_vegetable', [lambda d: ell(d, 48 * S, 74 * S, 12 * S, 9 * S, rgb((200, 140, 80))), lambda d: ell(d, 70 * S, 72 * S, 12 * S, 9 * S, rgb((170, 110, 180))),
                              lambda d: ell(d, 60 * S, 86 * S, 12 * S, 9 * S, rgb((220, 170, 70))), lambda d: ell(d, 82 * S, 86 * S, 10 * S, 8 * S, rgb((120, 160, 60)))])

    bowl('porridge', (230, 214, 170), [(56, 60, (180, 140, 90))])
    bowl('stew', (140, 70, 40), [(48, 60, (200, 120, 60)), (70, 64, (170, 110, 180)), (84, 58, (120, 60, 40))])
    bowl('fish_soup', (220, 200, 150), [(52, 60, (240, 230, 220)), (76, 62, (200, 140, 80))])
    bowl('mushroom_broth', (150, 110, 70), [(54, 60, (200, 190, 170)), (76, 62, (200, 190, 170))])
    plate('omelette', [lambda d: d.chord((30 * S, 56 * S, 98 * S, 104 * S), 180, 360, fill=rgb((246, 210, 90)), outline=INK, width=LW),
                       lambda d: ell(d, 64 * S, 72 * S, 6 * S, 4 * S, rgb((90, 140, 60)), width=2 * S)])
    plate('skewer', [lambda d: d.line([P(20, 86), P(108, 64)], fill=INK, width=5 * S)] +
          [(lambda x, y, c: (lambda d: ell(d, x * S, y * S, 10 * S, 9 * S, rgb(c))))(40 + k * 16, 82 - k * 4, c) for k, c in enumerate([(150, 70, 40), (200, 120, 60), (120, 160, 60), (150, 70, 40), (190, 40, 30)])])
    feast('hunters_feast')
    loaf('bread', (200, 140, 70))
    pie('pie', (150, 40, 60))
    pie('honey_cake', (236, 170, 40), cake=True)
    bowl('fire_pepper_stew', (170, 50, 30), [(50, 60, (200, 30, 20)), (72, 62, (176, 24, 16)), (84, 58, (120, 60, 40))])
    loaf('stormrye_flatbread', (196, 160, 100), flat=True)
    bowl('date_tagine', (160, 90, 40), [(50, 60, (120, 60, 25)), (70, 62, (190, 30, 110)), (84, 58, (110, 60, 40))])
    pie('idunn_pie', (210, 120, 30))
    bowl('cloudberry_porridge', (236, 220, 180), [(56, 60, (232, 150, 40)), (72, 62, (232, 150, 40))])
    bowl('glowmelon_compote', (140, 210, 180), [(52, 60, (238, 160, 176)), (74, 62, (110, 220, 190))])
    bowl('turnip_char_chowder', (226, 216, 196), [(52, 60, (190, 150, 200)), (74, 62, (240, 170, 140))])
    bowl('bog_rice_eel', (236, 232, 214), [(56, 60, (80, 70, 50)), (74, 62, (80, 70, 50))])
    plate('kelp_snapper', [lambda d: F.poly(d, [P(30, 80), P(60, 66), P(96, 74), P(108, 66), P(106, 92), P(96, 86), P(60, 94)], rgb((200, 60, 50)), INK, LW),
                           lambda d: d.rectangle((52 * S, 70 * S, 70 * S, 92 * S), fill=rgb((90, 80, 30)), outline=INK, width=3 * S)])
    loaf('mourners_bread', (70, 52, 40), score=(160, 140, 120))
    bowl('oyster_broth', (180, 140, 90), [(52, 60, (200, 190, 170)), (74, 62, (220, 110, 70))])
    loaf('grey_loaf', (150, 148, 140), score=(236, 230, 210))
    feast('banquet_portion', 4)

    slab('salted_meat', (190, 90, 80), crystals=(250, 250, 250))
    F.fish('salted_fish', rgb((170, 160, 150)), rgb((230, 226, 220)), rgb((150, 140, 130)), angle=-0.62) if (ALL or not os.path.exists(os.path.join(OUT, 'salted_fish.png'))) else None
    slab('smoked_meat', (110, 50, 30), string=True)
    F.fish('smoked_fish', rgb((120, 70, 30)), rgb((190, 130, 70)), rgb((90, 50, 20)), angle=-0.62) if (ALL or not os.path.exists(os.path.join(OUT, 'smoked_fish.png'))) else None
    slab('jerky', (120, 50, 35), strips=True)
    F.fish('dried_fish', rgb((180, 160, 120)), rgb((220, 205, 170)), rgb((150, 130, 100)), tail='fork', angle=-0.62) if (ALL or not os.path.exists(os.path.join(OUT, 'dried_fish.png'))) else None
    pile('dried_fruit', (200, 110, 50))
    pile('raisins', (70, 30, 40), r=6)

    LURES = {"gale_lure": ((150, 160, 190), (230, 220, 90)), "sun_lure": ((236, 190, 60), (250, 240, 200)), "grub_lure": ((150, 120, 70), (90, 140, 60)), "cloud_lure": ((220, 230, 245), (120, 160, 230)),
             "prism_lure": ((160, 110, 220), (110, 230, 230)), "mire_lure": ((120, 40, 40), (90, 110, 60)), "tide_lure": ((60, 140, 160), (240, 160, 120)), "grave_lure": ((120, 120, 120), (220, 220, 210)),
             "spore_lure": ((170, 120, 190), (230, 220, 170)), "void_lure": ((50, 30, 70), (170, 120, 230))}
    for k, (a, b) in LURES.items():
        lure(k, a, b)

    # The new fish (fish.json rows whose icons are missing), in the shared fish style; legends' trophies on boards.
    fishj = json.load(open(os.path.join(ROOT, 'Data', 'fish.json'), encoding='utf-8'))
    PAL = {"storm_peaks": ((90, 100, 120), (220, 222, 228), (150, 160, 180)), "desert": ((170, 140, 90), (230, 215, 180), (190, 120, 60)), "forest": ((80, 110, 70), (220, 210, 180), (150, 110, 80)),
           "sky_islands": ((150, 190, 230), (240, 245, 250), (120, 160, 220)), "crystal": ((200, 200, 220), (240, 240, 250), (170, 150, 220)), "swamp": ((80, 90, 50), (180, 170, 130), (100, 90, 50)),
           "ocean": ((70, 110, 150), (220, 225, 230), (200, 90, 70)), "graveyard": ((110, 110, 115), (210, 208, 200), (90, 90, 95)), "fungal": ((120, 200, 170), (230, 245, 235), (180, 120, 200)),
           "wasteland": ((60, 50, 70), (140, 130, 150), (120, 90, 160))}
    for f in fishj['fish']:
        item = f['item']
        if not ALL and os.path.exists(os.path.join(OUT, item + '.png')):
            pass
        else:
            look = f.get('look')
            back, belly, fin = (look['back'], look['belly'], look['fins']) if look else PAL.get(f['biome'], ((100, 100, 100), (220, 220, 220), (140, 140, 140)))
            long = 'eel' in f['id'] or f['id'] in ('lamprey', 'old_gar', 'prism_eel', 'null_eel', 'bog_eel', 'snakehead')
            F.fish(item, rgb(back), rgb(belly), rgb(fin), length=360 if long else 300, depth=70 if long else 120, tail='round' if long else 'fork',
                   dorsal='back' if long else ('sail' if f['id'] in ('lightning_pike', 'aethercarp', 'skyfin') else 'normal'))
        if f.get('rarity') == 'legendary' and f.get('look'):
            trophy('trophy_' + f['id'], f['look']['back'], f['look']['belly'], f['look']['fins'])


if __name__ == '__main__':
    main()
    print('farm icons written to', os.path.normpath(OUT))
