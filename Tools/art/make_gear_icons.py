"""Item icons for crafting gear (Data/UI/Icons/*.png, 128 px; cooks/build-crafting.md), in the UI's flat style of
make_farm_icons.py: thick dark outline, flat fills, one pale highlight, drawn at 4x and scaled down.
Steel and starsteel (ingots, plate, star iron, core shards, charcoal), every weapon class in every metal, the four armour
pieces of every tier and of the 12 biome sets (in each set's palette; Ascendant pieces carry the starsteel inlay),
rings (band in its metal, stone on top), talismans (the trophy bound in wire on a cord), the trophies, runes (a crystal
tablet with its glyph), blueprints (a rolled blue scroll with the boss's wax seal) and the other lands' materials.
Run: python Tools/art/make_gear_icons.py  (only writes icons that are missing; --all redraws them)."""
import json, math, os, sys
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_farm_icons as FI  # noqa: E402  (its helpers; its main() only runs as a script)

S, W, INK, HI = FI.S, FI.W, FI.INK, FI.HI
P, rgb, shade, poly, ell, canvas, done = FI.P, FI.rgb, FI.shade, FI.poly, FI.ell, FI.canvas, FI.done
LW = FI.LW
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')

METAL = {'copper': (190, 104, 60), 'bronze': (196, 150, 70), 'iron': (110, 112, 118), 'steel': (168, 176, 188), 'starsteel': (52, 62, 96)}
GEM = {'gem_ruby': (200, 30, 50), 'gem_topaz': (240, 180, 40), 'gem_onyx': (30, 28, 36)}


def glow(im, box, col, radius=8):
    g = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    ImageDraw.Draw(g).ellipse(tuple(v * S for v in box), fill=rgb(col, 120))
    im.alpha_composite(g.filter(ImageFilter.GaussianBlur(radius * S)), (0, 0))
    return ImageDraw.Draw(im)


# ---------------------------------------------------------------- metals and materials

def ingot(name, col, sheen=None):
    im, d = canvas()
    poly(d, [(22, 78), (40, 56), (104, 56), (110, 78)], rgb(shade(col, 1.2)))
    poly(d, [(22, 78), (110, 78), (104, 100), (28, 100)], rgb(col))
    d.line([P(44, 64), P(92, 64)], fill=HI, width=4 * S)
    if sheen:
        d.line([P(36, 90), P(70, 90)], fill=rgb(sheen), width=3 * S)
        d.line([P(78, 86), P(98, 86)], fill=rgb(sheen), width=3 * S)
    done(im, name)


def plate(name, col):
    im, d = canvas()
    for k, dy in enumerate((22, 0)):
        poly(d, [(18, 76 + dy - 10), (52, 52 + dy - 10), (110, 60 + dy - 10), (76, 86 + dy - 10)], rgb(shade(col, 1.0 - k * 0.12)))
        poly(d, [(18, 76 + dy - 10), (76, 86 + dy - 10), (76, 92 + dy - 10), (18, 82 + dy - 10)], rgb(shade(col, 0.7)), 3 * S)
    for (x, y) in [(36, 60), (90, 66)]:
        ell(d, x * S, y * S, 3 * S, 3 * S, rgb(shade(col, 0.5)), width=2 * S)
    d.line([P(42, 60), P(78, 50)], fill=HI, width=4 * S)
    done(im, name)


def lumps(name, col, flecks=None, n=6):
    im, d = canvas()
    pos = [(46, 90, 16), (74, 92, 18), (60, 70, 17), (88, 72, 13), (36, 70, 11), (62, 104, 10)]
    for (x, y, r) in pos[:n]:
        pts = [(x + r * math.cos(math.radians(a)) * (1 + 0.2 * math.sin(a * 3)), y + r * 0.8 * math.sin(math.radians(a))) for a in range(0, 360, 36)]
        poly(d, pts, rgb(col), 4 * S)
        d.line([P(x - r * 0.4, y - r * 0.4), P(x, y - r * 0.6)], fill=rgb(shade(col, 1.8) if col[0] < 60 else HI[:3]), width=3 * S)
        if flecks:
            for k in range(3):
                fx, fy = x + r * 0.5 * math.cos(k * 2.1 + x), y + r * 0.4 * math.sin(k * 2.1 + y)
                poly(d, [(fx, fy - 2.5), (fx + 2.5, fy), (fx, fy + 2.5), (fx - 2.5, fy)], rgb(flecks), 1 * S)
    done(im, name)


def shard(name, col, glow_col=None, dark=False):
    im, d = canvas()
    if glow_col:
        d = glow(im, (30, 24, 98, 112), glow_col, 10)
    poly(d, [(64, 14), (88, 54), (78, 114), (48, 112), (40, 58)], rgb(col))
    poly(d, [(64, 14), (88, 54), (64, 60)], rgb(shade(col, 1.25)), 3 * S)
    poly(d, [(64, 14), (64, 60), (40, 58)], rgb(shade(col, 0.85)), 3 * S)
    d.line([P(56, 40), P(52, 86)], fill=HI if not dark else (200, 180, 255, 180), width=4 * S)
    done(im, name)


def spine(name, col):
    im, d = canvas()
    poly(d, [(22, 108), (100, 20), (108, 28), (36, 116)], rgb(col))
    for t in (0.3, 0.5, 0.7):
        x, y = 22 + t * 80, 112 - t * 90
        d.line([P(x, y), P(x + 10, y + 12)], fill=INK, width=3 * S)
    d.line([P(40, 98), P(96, 32)], fill=HI, width=3 * S)
    done(im, name)


def hide(name, col, scale_col=None):
    im, d = canvas()
    pts = [(34, 30), (54, 38), (74, 30), (96, 36), (92, 58), (102, 80), (88, 104), (64, 96), (40, 106), (26, 82), (36, 60), (24, 44)]
    poly(d, pts, rgb(col))
    if scale_col:
        for (x, y) in [(50, 54), (66, 50), (80, 56), (44, 72), (60, 70), (76, 72), (90, 76), (54, 88), (70, 86)]:
            d.arc(((x - 7) * S, (y - 5) * S, (x + 7) * S, (y + 7) * S), 180, 360, fill=rgb(scale_col), width=3 * S)
    d.line([P(40, 50), P(52, 44)], fill=HI, width=4 * S)
    done(im, name)


def cloth(name, col, stripe=None):
    im, d = canvas()
    for k in range(3):
        y = 56 + k * 16
        poly(d, [(24, y), (100, y - 8), (104, y + 12), (28, y + 20)], rgb(shade(col, 1.0 - k * 0.08)), 4 * S)
        if stripe:
            d.line([P(30, y + 8), P(100, y)], fill=rgb(stripe), width=3 * S)
    d.line([P(34, 58), P(80, 52)], fill=HI, width=3 * S)
    done(im, name)


def log(name, bark, core):
    im, d = canvas()
    poly(d, [(26, 66), (88, 30), (108, 58), (46, 96)], rgb(bark))
    ell(d, 36 * S, 82 * S, 18 * S, 16 * S, rgb(shade(core, 0.8)))
    ell(d, 36 * S, 82 * S, 9 * S, 8 * S, rgb(core), width=3 * S)
    d.line([P(54, 54), P(86, 36)], fill=HI, width=3 * S)
    done(im, name)


def coral(name, col):
    im, d = canvas()
    for (x0, y0, x1, y1) in [(64, 112, 64, 60), (64, 80, 40, 50), (64, 70, 90, 40), (40, 50, 30, 30), (40, 50, 50, 26), (90, 40, 100, 22), (90, 40, 80, 20)]:
        d.line([P(x0, y0), P(x1, y1)], fill=INK, width=14 * S)
        d.line([P(x0, y0), P(x1, y1)], fill=rgb(col), width=7 * S)
    done(im, name)


def spool(name, col):
    im, d = canvas()
    d.rectangle((40 * S, 30 * S, 88 * S, 40 * S), fill=rgb((140, 100, 60)), outline=INK, width=4 * S)
    d.rectangle((40 * S, 96 * S, 88 * S, 106 * S), fill=rgb((140, 100, 60)), outline=INK, width=4 * S)
    d.rectangle((46 * S, 40 * S, 82 * S, 96 * S), fill=rgb(col), outline=INK, width=4 * S)
    for y in range(46, 94, 8):
        d.line([P(48, y), P(80, y + 3)], fill=rgb(shade(col, 0.8)), width=2 * S)
    done(im, name)


def pouch(name, col):
    im, d = canvas()
    poly(d, [(36, 108), (30, 72), (46, 52), (82, 52), (98, 72), (92, 108)], rgb((170, 140, 100)))
    d.line([P(44, 54), P(84, 54)], fill=INK, width=4 * S)
    for (x, y) in [(56, 44), (66, 38), (76, 44), (62, 30)]:
        ell(d, x * S, y * S, 5 * S, 5 * S, rgb(col), width=2 * S)
    done(im, name)


def stone(name, col, glyph=None):
    im, d = canvas()
    pts = [(28, 80), (36, 52), (62, 40), (92, 48), (104, 76), (88, 104), (48, 106)]
    poly(d, pts, rgb(col))
    if glyph:
        d.line([P(56, 60), P(66, 90)], fill=rgb(glyph), width=4 * S)
        d.line([P(66, 60), P(76, 74)], fill=rgb(glyph), width=4 * S)
    d.line([P(42, 60), P(56, 50)], fill=HI, width=4 * S)
    done(im, name)


def moss(name, col):
    im, d = canvas()
    for (x, y, r) in [(48, 82, 22), (74, 76, 24), (60, 64, 20), (86, 92, 16), (40, 98, 14)]:
        ell(d, x * S, y * S, r * S, r * 0.8 * S, rgb(col), width=4 * S)
    for (x, y) in [(50, 76), (70, 70), (82, 86), (58, 90)]:
        d.ellipse(((x - 3) * S, (y - 3) * S, (x + 3) * S, (y + 3) * S), fill=rgb(shade(col, 1.4)))
    done(im, name)


# ---------------------------------------------------------------- weapons

def weapon(name, cls, col, accent=None, glow_col=None):
    im, d = canvas()
    if glow_col:
        # The head glows: a hammer's head and a spear's tip sit up and left.
        d = glow(im, (4, 4, 84, 84) if cls == 'great_hammer' else (0, 0, 60, 60), glow_col, 12)
    haft = (110, 74, 40)
    if cls == 'sword' or cls == 'greatsword':
        L = 1.0 if cls == 'sword' else 1.15
        tip = (20 + (1 - L) * 20, 20 + (1 - L) * 20)
        poly(d, [(tip[0], tip[1]), (tip[0] + 12, tip[1] + 2), (82, 72), (74, 80), (tip[0] + 2, tip[1] + 12)], rgb(col))
        d.line([P(tip[0] + 8, tip[1] + 8), P(74, 70)], fill=HI, width=3 * S)
        poly(d, [(64, 92), (92, 64), (98, 70), (70, 98)], rgb(accent or (90, 70, 40)), 4 * S)
        d.line([P(82, 82), P(104, 104)], fill=INK, width=12 * S)
        d.line([P(82, 82), P(104, 104)], fill=rgb(haft), width=6 * S)
        ell(d, 108 * S, 108 * S, 6 * S, 6 * S, rgb(accent or col), width=3 * S)
    elif cls == 'dagger':
        poly(d, [(38, 38), (50, 40), (80, 72), (72, 80), (40, 50)], rgb(col))
        d.line([P(46, 46), P(74, 72)], fill=HI, width=3 * S)
        poly(d, [(64, 88), (88, 64), (94, 70), (70, 94)], rgb(accent or (90, 70, 40)), 4 * S)
        d.line([P(80, 80), P(98, 98)], fill=INK, width=12 * S)
        d.line([P(80, 80), P(98, 98)], fill=rgb(haft), width=6 * S)
    elif cls == 'spear':
        d.line([P(30, 30), P(110, 110)], fill=INK, width=10 * S)
        d.line([P(30, 30), P(110, 110)], fill=rgb(haft), width=5 * S)
        poly(d, [(14, 14), (38, 22), (44, 44), (22, 38)], rgb(col))
        d.line([P(20, 20), P(38, 38)], fill=HI, width=3 * S)
        if accent:
            d.line([P(40, 40), P(48, 48)], fill=rgb(accent), width=6 * S)
    elif cls == 'great_hammer':
        d.line([P(48, 48), P(112, 112)], fill=INK, width=12 * S)
        d.line([P(48, 48), P(112, 112)], fill=rgb(haft), width=6 * S)
        poly(d, [(16, 46), (46, 16), (72, 42), (42, 72)], rgb(col))
        poly(d, [(16, 46), (46, 16), (52, 22), (22, 52)], rgb(shade(col, 1.25)), 3 * S)
        if accent:
            d.line([P(34, 50), P(50, 34)], fill=rgb(accent), width=5 * S)
    elif cls == 'scythe':
        d.line([P(24, 30), P(108, 114)], fill=INK, width=10 * S)
        d.line([P(24, 30), P(108, 114)], fill=rgb(haft), width=5 * S)
        pts = [(26, 32), (40, 18), (66, 14), (94, 22)]
        inner = [(90, 30), (64, 24), (44, 28), (32, 40)]
        poly(d, pts + inner, rgb(col))
        d.line([P(44, 20), P(84, 22)], fill=HI, width=3 * S)
    done(im, name)


# ---------------------------------------------------------------- armour pieces

def armour(name, part, base, trim, inlay=None, hood=False, fur=None):
    im, d = canvas()
    if part == 'head':
        if hood:
            poly(d, [(30, 96), (34, 50), (52, 28), (76, 28), (94, 50), (98, 96), (80, 104), (64, 84), (48, 104)], rgb(base))
            poly(d, [(46, 66), (82, 66), (78, 92), (50, 92)], rgb(shade(base, 0.35)), 3 * S)
        else:
            d.chord((30 * S, 26 * S, 98 * S, 100 * S), 180, 360, fill=rgb(base), outline=INK, width=LW)
            poly(d, [(30, 63), (98, 63), (94, 96), (78, 100), (74, 74), (54, 74), (50, 100), (34, 96)], rgb(shade(base, 0.85)))
            d.line([P(64, 28), P(64, 62)], fill=rgb(trim), width=6 * S)
            d.line([P(54, 80), P(74, 80)], fill=INK, width=4 * S)
        d.line([P(40, 52), P(52, 38)], fill=HI, width=4 * S)
    elif part == 'armor':
        poly(d, [(30, 30), (50, 24), (64, 34), (78, 24), (98, 30), (106, 56), (92, 60), (90, 110), (38, 110), (36, 60), (22, 56)], rgb(base))
        d.line([P(64, 36), P(64, 108)], fill=rgb(shade(base, 0.7)), width=4 * S)
        poly(d, [(38, 84), (90, 84), (90, 92), (38, 92)], rgb(trim), 3 * S)
        if fur:
            for x in range(34, 96, 10):
                ell(d, x * S, 30 * S, 7 * S, 6 * S, rgb(fur), width=2 * S)
        d.line([P(44, 44), P(54, 66)], fill=HI, width=4 * S)
    elif part == 'hands':
        poly(d, [(40, 110), (36, 70), (44, 40), (52, 38), (54, 60), (58, 30), (66, 30), (68, 58), (72, 32), (80, 34), (80, 62), (86, 46), (94, 50), (88, 80), (86, 110)], rgb(base))
        poly(d, [(38, 94), (88, 94), (86, 110), (40, 110)], rgb(trim), 3 * S)
        d.line([P(46, 56), P(46, 80)], fill=HI, width=3 * S)
    else:  # legs
        poly(d, [(36, 22), (92, 22), (90, 50), (76, 106), (92, 108), (92, 116), (62, 116), (66, 54), (60, 54), (54, 116), (26, 116), (26, 108), (42, 106), (38, 50)], rgb(base))
        d.line([P(36, 30), P(92, 30)], fill=rgb(trim), width=6 * S)
        poly(d, [(26, 104), (54, 104), (54, 116), (26, 116)], rgb(shade(base, 0.7)), 3 * S)
        poly(d, [(62, 104), (92, 104), (92, 116), (62, 116)], rgb(shade(base, 0.7)), 3 * S)
        d.line([P(44, 40), P(46, 80)], fill=HI, width=3 * S)
    if inlay:
        d = glow(im, (44, 44, 84, 84), inlay, 6)
        d.line([P(52, 52), P(76, 76)], fill=rgb(inlay), width=3 * S)
        d.line([P(76, 52), P(52, 76)], fill=rgb(inlay), width=3 * S)
    done(im, name)


SETS = {  # base, trim, hood, fur (the hero cook's palettes)
    'forgeplate': ((40, 36, 36), (226, 84, 26), False, None),
    'stormweave': ((86, 94, 108), (185, 167, 255), True, (150, 140, 130)),
    'dunewrap': ((214, 190, 140), (232, 178, 48), True, None),
    'heartguard': ((84, 110, 50), (200, 140, 50), False, None),
    'skysilk': ((232, 238, 246), (140, 190, 230), True, None),
    'wardcloth': ((70, 40, 120), (230, 230, 255), True, None),
    'rimeguard': ((200, 214, 226), (140, 200, 240), False, (246, 248, 250)),
    'mirehide': ((30, 46, 34), (142, 15, 30), False, None),
    'reefmail': ((226, 130, 130), (30, 140, 134), False, None),
    'gravecloth': ((34, 32, 36), (192, 192, 200), True, None),
    'sporeweave': ((110, 80, 52), (57, 230, 194), True, None),
    'hollowplate': ((20, 18, 26), (120, 70, 200), False, None),
}
OLD = {'hide': ((140, 96, 60), (90, 60, 36), True, (200, 180, 150)), 'copper': (METAL['copper'], (60, 140, 110), False, None),
       'bronze': (METAL['bronze'], (150, 40, 40), False, None), 'iron': (METAL['iron'], (60, 60, 70), False, None)}


# ---------------------------------------------------------------- rings, talismans, trophies, runes, blueprints

def ring(name, metal, gem):
    im, d = canvas()
    ell(d, 64 * S, 78 * S, 34 * S, 26 * S, rgb(METAL[metal]), width=LW)
    ell(d, 64 * S, 80 * S, 22 * S, 15 * S, (0, 0, 0, 0), width=LW)
    d.rectangle((50 * S, 36 * S, 78 * S, 56 * S), fill=rgb(shade(METAL[metal], 0.8)), outline=INK, width=4 * S)
    poly(d, [(64, 22), (80, 38), (64, 54), (48, 38)], rgb(GEM[gem]), 4 * S)
    d.line([P(58, 32), P(62, 28)], fill=HI, width=3 * S)
    d.arc((34 * S, 56 * S, 94 * S, 104 * S), 200, 250, fill=HI, width=3 * S)
    done(im, name)


def charm_shape(d, kind, col, cx=64, cy=78, k=1.0):
    if kind == 'fang' or kind == 'tooth' or kind == 'claw' or kind == 'stinger' or kind == 'spine':
        poly(d, [(cx - 12 * k, cy - 20 * k), (cx + 12 * k, cy - 20 * k), (cx + 4 * k, cy + 28 * k), (cx - 2 * k, cy + 30 * k)], rgb(col))
    elif kind == 'horn' or kind == 'antler':
        pts = [(cx - 14 * k, cy - 18 * k), (cx + 6 * k, cy - 22 * k), (cx + 22 * k, cy + 14 * k), (cx + 12 * k, cy + 30 * k), (cx + 4 * k, cy + 4 * k)]
        poly(d, pts, rgb(col))
        if kind == 'antler':
            d.line([P(cx + 8 * k, cy - 4 * k), P(cx + 24 * k, cy - 12 * k)], fill=INK, width=8 * S)
            d.line([P(cx + 8 * k, cy - 4 * k), P(cx + 24 * k, cy - 12 * k)], fill=rgb(col), width=4 * S)
    elif kind == 'crown':
        poly(d, [(cx - 22 * k, cy + 10 * k), (cx - 22 * k, cy - 14 * k), (cx - 10 * k, cy), (cx, cy - 20 * k), (cx + 10 * k, cy), (cx + 22 * k, cy - 14 * k), (cx + 22 * k, cy + 10 * k)], rgb(col))
    elif kind == 'eye' or kind == 'pearl':
        ell(d, cx * S, cy * S, 18 * k * S, 18 * k * S, rgb(col))
        if kind == 'eye':
            ell(d, cx * S, cy * S, 6 * k * S, 9 * k * S, rgb(INK[:3]), width=2 * S)
        d.arc(((cx - 12 * k) * S, (cy - 12 * k) * S, (cx + 4 * k) * S, (cy + 4 * k) * S), 190, 250, fill=HI, width=3 * S)
    elif kind == 'plate' or kind == 'shard' or kind == 'seal' or kind == 'stone' or kind == 'mask':
        poly(d, [(cx, cy - 22 * k), (cx + 20 * k, cy - 4 * k), (cx + 12 * k, cy + 22 * k), (cx - 12 * k, cy + 22 * k), (cx - 20 * k, cy - 4 * k)], rgb(col))
        if kind == 'mask':
            for sx in (-8, 8):
                ell(d, (cx + sx * k) * S, (cy - 2 * k) * S, 4 * k * S, 3 * k * S, rgb(INK[:3]), width=1 * S)
    elif kind == 'heart' or kind == 'marrow' or kind == 'sac' or kind == 'cap':
        ell(d, (cx - 9 * k) * S, (cy - 6 * k) * S, 12 * k * S, 12 * k * S, rgb(col))
        ell(d, (cx + 9 * k) * S, (cy - 6 * k) * S, 12 * k * S, 12 * k * S, rgb(col))
        poly(d, [(cx - 20 * k, cy - 2 * k), (cx + 20 * k, cy - 2 * k), (cx, cy + 24 * k)], rgb(col))
    elif kind == 'plume' or kind == 'silk' or kind == 'thread':
        poly(d, [(cx - 4 * k, cy + 28 * k), (cx - 16 * k, cy - 6 * k), (cx, cy - 26 * k), (cx + 14 * k, cy - 6 * k), (cx + 4 * k, cy + 28 * k)], rgb(col))
        d.line([P(cx, cy - 22 * k), P(cx, cy + 26 * k)], fill=INK, width=3 * S)
    elif kind == 'scale' or kind == 'fin':
        poly(d, [(cx - 20 * k, cy + 20 * k), (cx, cy - 24 * k), (cx + 20 * k, cy + 20 * k)], rgb(col))
    elif kind == 'clapper' or kind == 'spade' or kind == 'fork':
        d.line([P(cx, cy - 26 * k), P(cx, cy + 10 * k)], fill=INK, width=8 * S)
        d.line([P(cx, cy - 26 * k), P(cx, cy + 10 * k)], fill=rgb(col), width=4 * S)
        ell(d, cx * S, (cy + 16 * k) * S, 12 * k * S, 12 * k * S, rgb(col))
    else:
        ell(d, cx * S, cy * S, 18 * k * S, 22 * k * S, rgb(col))


def talisman(name, kind, col):
    im, d = canvas()
    # A leather cord in a V down to the bail, the charm bound under it in bronze wire.
    for x0 in (30, 98):
        d.line([P(x0, 8), P(64, 48)], fill=INK, width=7 * S)
        d.line([P(x0, 8), P(64, 48)], fill=rgb((150, 110, 70)), width=3 * S)
    ell(d, 64 * S, 50 * S, 6 * S, 6 * S, rgb(METAL['bronze']), width=3 * S)
    charm_shape(d, kind, col, 64, 82, 1.0)
    for y in (70, 78):
        d.line([P(50, y), P(78, y + 4)], fill=rgb(METAL['bronze']), width=3 * S)
    done(im, name)


def trophy_icon(name, kind, col):
    im, d = canvas()
    charm_shape(d, kind, col, 64, 66, 1.6)
    done(im, name)


def rune(name, glyph_col):
    im, d = canvas()
    d = glow(im, (30, 20, 98, 110), glyph_col, 8)
    d.rounded_rectangle((34 * S, 22 * S, 94 * S, 108 * S), radius=14 * S, fill=rgb((110, 80, 170)), outline=INK, width=LW)
    d.line([P(64, 36), P(64, 94)], fill=rgb(glyph_col), width=6 * S)
    d.line([P(64, 50), P(82, 38)], fill=rgb(glyph_col), width=6 * S)
    d.line([P(64, 70), P(46, 58)], fill=rgb(glyph_col), width=6 * S)
    d.line([P(42, 32), P(42, 52)], fill=HI, width=3 * S)
    done(im, name)


def blueprint(name, seal):
    im, d = canvas()
    poly(d, [(22, 40), (98, 28), (106, 90), (30, 102)], rgb((60, 96, 160)))
    for k in range(4):
        d.line([P(36, 52 + k * 11), P(88, 44 + k * 11)], fill=rgb((210, 228, 250)), width=2 * S)
    ell(d, 26 * S, 72 * S, 10 * S, 32 * S, rgb((50, 80, 140)))
    ell(d, 102 * S, 60 * S, 10 * S, 32 * S, rgb((50, 80, 140)))
    ell(d, 76 * S, 88 * S, 13 * S, 13 * S, rgb(seal), width=4 * S)
    d.line([P(70, 84), P(80, 92)], fill=rgb(shade(seal, 0.6)), width=3 * S)
    done(im, name)


def kind_of(trophy_id):
    for k in ('fang', 'tooth', 'claw', 'stinger', 'horn', 'antler', 'crown', 'eye', 'pearl', 'plate', 'shard', 'seal', 'stone', 'mask',
              'heart', 'marrow', 'sac', 'cap', 'plume', 'silk', 'thread', 'scale', 'fin', 'clapper', 'spade', 'fork', 'tusk', 'maw', 'sting', 'ivory'):
        if k in trophy_id:
            return {'tusk': 'horn', 'ivory': 'horn', 'maw': 'fang', 'sting': 'stinger'}.get(k, k)
    return 'stone'


BIOME_COL = {'volcanic': (226, 84, 26), 'storm_peaks': (185, 167, 255), 'desert': (232, 178, 48), 'forest': (94, 140, 49), 'sky_islands': (159, 214, 207),
             'crystal': (122, 63, 209), 'tundra': (140, 200, 240), 'swamp': (142, 15, 30), 'ocean': (30, 140, 134), 'graveyard': (168, 184, 154),
             'fungal': (57, 230, 194), 'wasteland': (90, 60, 140)}


def main():
    items = json.load(open(os.path.join(ROOT, 'Data', 'items.json'), encoding='utf-8'))['items']
    byid = {i['id']: i for i in items if 'id' in i}
    ingot('steel_ingot', METAL['steel'])
    ingot('starsteel_ingot', METAL['starsteel'], sheen=(150, 180, 255))
    ingot('star_iron', (70, 78, 100), sheen=(200, 215, 240))
    plate('steel_plate', METAL['steel'])
    lumps('charcoal', (26, 24, 24))
    lumps('star_iron_ore', (40, 44, 60), flecks=(210, 225, 250))
    shard('core_shard', (250, 170, 90), glow_col=(255, 140, 40))
    shard('obsidian', (24, 20, 30), dark=True)
    hide('magmaw_hide', (70, 30, 24), scale_col=(226, 84, 26))
    spine('rimeclaw_spine', (170, 210, 240))
    cloth('linen', (226, 216, 190))
    # the other lands' materials
    shard('stormglass', (120, 120, 150), glow_col=(185, 167, 255))
    shard('sunstone', (232, 170, 50), glow_col=(255, 200, 80))
    log('heartwood', (90, 64, 40), (220, 150, 50))
    stone('levistone', (210, 220, 228))
    shard('resonant_crystal', (150, 100, 230), glow_col=(170, 120, 255))
    moss('bloodmoss', (150, 24, 34))
    coral('tidecoral', (232, 120, 120))
    stone('soulstone', (130, 140, 120), glyph=(190, 230, 190))
    FI.mushroom('deepcap', (40, 200, 180), (220, 220, 200))
    shard('voidglass', (20, 16, 30), glow_col=(120, 70, 200), dark=True)
    FI.feather('puffram_wool', (200, 200, 214), fluffy=True)
    hide('dromad_leather', (190, 150, 100))
    FI.feather('fernsheep_wool', (170, 200, 140), fluffy=True)
    FI.jar('resin', (220, 150, 40), small=True)
    FI.feather('puffwing_feathers', (240, 240, 250))
    pouch('chimecrab_dust', (200, 180, 250))
    hide('murkjaw_hide', (34, 50, 38), scale_col=(80, 110, 80))
    FI.jar('slimeshell_slime', (130, 190, 90))
    shard('spinecrab_shell', (200, 90, 70))
    hide('mournhare_fur', (150, 150, 156))
    spool('webspinner_silk', (236, 236, 226))
    FI.jar('puffball_sac', (200, 180, 140), small=True)
    hide('withergoat_hide', (120, 116, 110))
    # weapons
    for metal in ('steel', 'starsteel'):
        for cls in ('sword', 'dagger', 'spear', 'greatsword', 'great_hammer', 'scythe'):
            weapon(f'{metal}_{cls}', cls, METAL[metal], accent=(150, 180, 255) if metal == 'starsteel' else None)
    weapon('cinderbrand', 'great_hammer', (60, 50, 50), accent=(255, 120, 40), glow_col=(255, 100, 30))
    weapon('rimefang', 'spear', (200, 230, 250), accent=(140, 200, 240), glow_col=(150, 210, 255))
    blueprint('bp_cinderbrand', (200, 60, 30))
    blueprint('bp_rimefang', (90, 150, 220))
    # armour
    for tier, (base, trim, hood, fur) in OLD.items():
        for part in ('head', 'hands', 'legs'):
            armour(f'{tier}_{part}', part, base, trim, hood=hood, fur=fur)
    for sid, (base, trim, hood, fur) in SETS.items():
        for asc in (False, True):
            stem = f'{sid}_ascendant' if asc else sid
            for part in ('head', 'armor', 'hands', 'legs'):
                pid = f'{stem}_armor' if part == 'armor' else f'{stem}_{part}'
                armour(pid, part, base, trim, inlay=(150, 190, 255) if asc else None, hood=hood, fur=fur)
    # rings
    for metal in METAL:
        for g in ('ruby', 'topaz', 'onyx'):
            ring(f'ring_{metal}_{g}', metal, f'gem_{g}')
    # trophies and talismans
    for i in items:
        iid = i.get('id', '')
        col = BIOME_COL.get(i.get('biome', ''), (220, 200, 170))
        if i.get('category') == 'trophy' and iid.startswith('trophy_'):
            trophy_icon(iid, kind_of(iid), col)
        if i.get('category') == 'talisman':
            src = iid.replace('talisman_', 'trophy_')
            talisman(iid, kind_of(src) if src in byid else 'stone', col)
    # runes
    for rid, col in (('thirst', (230, 40, 60)), ('haste', (185, 167, 255)), ('anvil', (250, 200, 60)), ('tide', (60, 200, 190)), ('thorns', (120, 200, 80)),
                     ('grave', (190, 230, 190)), ('sight', (255, 220, 120)), ('hollowing', (160, 100, 255))):
        rune(f'rune_{rid}', col)


if __name__ == '__main__':
    main()
    print('gear icons written to', os.path.normpath(FI.OUT))
