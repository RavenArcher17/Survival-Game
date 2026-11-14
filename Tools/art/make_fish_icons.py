"""Item icons for fishing (Data/UI/Icons/*.png, 128 px), drawn in the UI's flat style: a thick dark outline, flat
fills, one pale highlight. Drawn at 4x and scaled down for clean edges. Run: python Tools/art/make_fish_icons.py
Every fish is laid head up-right on the diagonal like the other icons, with its own silhouette and markings."""
import math, os
from PIL import Image, ImageDraw

OUT = os.path.join(os.path.dirname(__file__), '..', '..', 'Data', 'UI', 'Icons')
S = 4
W = 128 * S
INK = (38, 27, 20, 255)
HI = (255, 246, 228, 200)


def canvas():
    im = Image.new('RGBA', (W, W), (0, 0, 0, 0))
    return im, ImageDraw.Draw(im)


def save(im, name, zoom=1.0):
    if zoom != 1.0:
        # Drawn small in the middle: crop to it and scale up, so it fills the icon like the others.
        h = W / zoom / 2
        im = im.crop((int(W / 2 - h), int(W / 2 - h), int(W / 2 + h), int(W / 2 + h))).resize((W, W), Image.LANCZOS)
    im.resize((128, 128), Image.LANCZOS).save(os.path.join(OUT, name + '.png'))


def rot(pts, ang, cx=W / 2, cy=W / 2):
    c, s = math.cos(ang), math.sin(ang)
    return [(cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c) for x, y in pts]


def poly(d, pts, fill, outline=INK, width=5 * S):
    d.polygon(pts, fill=fill)
    d.line(pts + [pts[0]], fill=outline, width=width, joint='curve')


def body_pts(length, depth, cx, cy, head_round=1.0, tail_pinch=0.18, belly=1.0, back=1.0, n=40):
    """A fish body outline along X: deepest a third back from the head, pinched at the tail."""
    top, bot = [], []
    for i in range(n + 1):
        t = i / n                     # 0 tail .. 1 head
        x = cx - length / 2 + t * length
        shape = math.sin(math.pi * (t ** 0.85)) ** (0.75 / head_round)
        h = depth / 2 * max(tail_pinch, shape)
        top.append((x, cy - h * back))
        bot.append((x, cy + h * belly))
    return top + bot[::-1]


def fish(name, back, belly, fin, length=300, depth=120, tail='fork', dorsal='normal', marks=None, eye=(250, 240, 220), angle=-0.62,
         head_round=1.0, extra=None, glow=None):
    im, d = canvas()
    cx, cy = W / 2 + 6 * S, W / 2 + 4 * S
    L, D = length * S / 4 * 1.0, depth * S / 4
    shapes = []
    # Tail fin
    tx = cx - L / 2
    if tail == 'fork':
        shapes.append(([(tx + 10 * S, cy), (tx - 34 * S, cy - D * 0.62), (tx - 18 * S, cy), (tx - 34 * S, cy + D * 0.62)], fin))
    elif tail == 'round':
        shapes.append(([(tx + 10 * S, cy - 6 * S), (tx - 26 * S, cy - D * 0.5), (tx - 34 * S, cy), (tx - 26 * S, cy + D * 0.5), (tx + 10 * S, cy + 6 * S)], fin))
    elif tail == 'lunate':
        shapes.append(([(tx + 10 * S, cy), (tx - 30 * S, cy - D * 0.9), (tx - 14 * S, cy), (tx - 30 * S, cy + D * 0.9)], fin))
    # Dorsal fin
    if dorsal == 'normal':
        shapes.append(([(cx - L * 0.12, cy - D * 0.42), (cx + L * 0.02, cy - D * 0.82), (cx + L * 0.16, cy - D * 0.44)], fin))
    elif dorsal == 'sail':
        shapes.append(([(cx - L * 0.28, cy - D * 0.36), (cx - L * 0.1, cy - D * 1.25), (cx + L * 0.22, cy - D * 1.05), (cx + L * 0.24, cy - D * 0.42)], fin))
    elif dorsal == 'back':
        shapes.append(([(cx - L * 0.38, cy - D * 0.2), (cx - L * 0.28, cy - D * 0.62), (cx - L * 0.18, cy - D * 0.34)], fin))
    # Under fins
    shapes.append(([(cx - L * 0.02, cy + D * 0.44), (cx - L * 0.14, cy + D * 0.8), (cx + L * 0.1, cy + D * 0.46)], fin))
    for pts, col in shapes:
        poly(d, rot(pts, angle), col)
    bp = body_pts(L, D, cx, cy, head_round)
    poly(d, rot(bp, angle), back)
    # Belly: the lower half lighter (a second polygon inside the body, no outline)
    belly_pts = [(x, y) for x, y in bp if y >= cy - 1]
    if belly is not None and len(belly_pts) > 3:
        lower = [(x, max(y, cy + D * 0.06)) for x, y in bp[len(bp) // 2:]]
        upper = [(x, cy + D * 0.06) for x, y in lower[::-1]]
        d.polygon(rot(lower + upper, angle), fill=belly)
    if marks:
        marks(d, cx, cy, L, D, angle)
    # Gill line, eye, highlight
    gx = cx + L * 0.28
    d.line(rot([(gx, cy - D * 0.3), (gx - 8 * S, cy), (gx, cy + D * 0.3)], angle), fill=INK, width=3 * S, joint='curve')
    ex, ey = rot([(cx + L * 0.38, cy - D * 0.12)], angle)[0]
    r = 7 * S
    d.ellipse((ex - r, ey - r, ex + r, ey + r), fill=eye, outline=INK, width=3 * S)
    d.ellipse((ex - r * 0.45, ey - r * 0.45, ex + r * 0.45, ey + r * 0.45), fill=INK)
    d.line(rot([(cx - L * 0.2, cy - D * 0.32), (cx + L * 0.12, cy - D * 0.38)], angle), fill=HI, width=4 * S)
    # Redraw the body outline on top of the markings
    d.line(rot(bp, angle) + [rot(bp, angle)[0]], fill=INK, width=5 * S, joint='curve')
    if extra:
        extra(d, cx, cy, L, D, angle)
    if glow:
        glow(im)
    save(im, name)


def stripes(col, n=5, slant=0.35):
    def f(d, cx, cy, L, D, a):
        for i in range(n):
            x = cx - L * 0.3 + i * L * 0.12
            d.line(rot([(x, cy - D * 0.38), (x + slant * D, cy - D * 0.05)], a), fill=col, width=5 * S)
    return f


def spots(col, n=14, seed=3, low=-0.36, high=0.05, size=4):
    def f(d, cx, cy, L, D, a):
        k = seed
        for i in range(n):
            k = (k * 1103515245 + 12345) & 0x7fffffff
            u = (k % 1000) / 1000
            k = (k * 1103515245 + 12345) & 0x7fffffff
            v = (k % 1000) / 1000
            x = cx - L * 0.32 + u * L * 0.6
            y = cy + D * (low + v * (high - low))
            px, py = rot([(x, y)], a)[0]
            r = size * S
            d.ellipse((px - r, py - r, px + r, py + r), fill=col)
    return f


def bars(col, n=6):
    def f(d, cx, cy, L, D, a):
        for i in range(n):
            x = cx - L * 0.34 + i * L * 0.11
            d.line(rot([(x, cy - D * 0.4), (x - 4 * S, cy + D * 0.25)], a), fill=col, width=6 * S)
    return f


def both(*fs):
    def f(d, cx, cy, L, D, a):
        for g in fs:
            g(d, cx, cy, L, D, a)
    return f


def cracks(col):
    def f(d, cx, cy, L, D, a):
        for pts in ([(cx - L * 0.3, cy - D * 0.1), (cx - L * 0.15, cy - D * 0.25), (cx, cy - D * 0.05), (cx + L * 0.15, cy - D * 0.2)],
                    [(cx - L * 0.22, cy + D * 0.15), (cx - L * 0.05, cy + D * 0.05), (cx + L * 0.1, cy + D * 0.2)]):
            d.line(rot(pts, a), fill=col, width=4 * S, joint='curve')
    return f


def bill(d, cx, cy, L, D, a):
    poly(d, rot([(cx + L * 0.46, cy - D * 0.06), (cx + L * 0.46 + 52 * S, cy - D * 0.18), (cx + L * 0.46, cy + D * 0.08)], a), (70, 54, 60, 255), width=4 * S)


def scutes(d, cx, cy, L, D, a):
    for i in range(7):
        x = cx - L * 0.32 + i * L * 0.1
        px, py = rot([(x, cy - D * 0.36)], a)[0]
        r = 4 * S
        d.polygon([(px, py - r), (px + r, py), (px, py + r), (px - r, py)], fill=(232, 226, 210, 255), outline=INK)


# ---- the fish
fish('fish_cinder_snapper', (196, 74, 48, 255), (236, 168, 120, 255), (150, 44, 30, 255))
fish('fish_ashfin_mackerel', (92, 96, 102, 255), (214, 210, 200, 255), (60, 62, 66, 255), length=320, depth=100, tail='lunate', marks=stripes((40, 40, 44, 255), 6))
fish('fish_steamfin_trout', (128, 112, 80, 255), (226, 196, 160, 255), (98, 84, 60, 255), length=310, depth=105, tail='round', marks=spots((60, 40, 30, 255), 16, 5))
fish('fish_sulfur_carp', (206, 170, 48, 255), (240, 222, 140, 255), (168, 128, 30, 255), length=280, depth=140, tail='fork', head_round=1.3,
     marks=spots((176, 140, 30, 255), 22, 9, -0.4, 0.3, 3))
fish('fish_cinder_marlin', (40, 52, 84, 255), (190, 200, 210, 255), (30, 38, 64, 255), length=320, depth=100, tail='lunate', dorsal='sail', extra=bill)
fish('fish_rime_cod', (150, 140, 112, 255), (226, 222, 206, 255), (118, 108, 84, 255), length=300, depth=125, tail='round', marks=spots((104, 94, 72, 255), 18, 11))
fish('fish_glassfin_herring', (110, 150, 180, 255), (226, 236, 242, 255), (150, 190, 214, 200), length=300, depth=95, tail='fork')
fish('fish_aurora_char', (52, 112, 96, 255), (236, 120, 120, 255), (200, 80, 90, 255), length=300, depth=110, tail='fork',
     marks=spots((240, 200, 120, 255), 12, 21, -0.38, -0.05, 3.5))
fish('fish_silver_grayling', (130, 140, 152, 255), (220, 226, 232, 255), (120, 90, 130, 255), length=300, depth=105, tail='fork', dorsal='sail',
     marks=spots((60, 62, 70, 255), 8, 13, -0.3, 0.0, 3))
fish('fish_frostjaw_pike', (84, 112, 82, 255), (220, 220, 190, 255), (70, 86, 60, 255), length=340, depth=90, tail='round', dorsal='back', marks=bars((54, 78, 52, 255), 7))
fish('fish_pale_sturgeon', (150, 150, 140, 255), (220, 216, 204, 255), (120, 118, 110, 255), length=340, depth=95, tail='lunate', dorsal='back', extra=scutes)
fish('fish_emberking', (46, 30, 26, 255), (120, 52, 30, 255), (220, 96, 30, 255), length=310, depth=140, tail='lunate', dorsal='sail',
     marks=cracks((255, 150, 40, 255)), eye=(255, 190, 60))
fish('fish_old_whitegill', (210, 216, 220, 255), (244, 246, 246, 255), (176, 186, 196, 255), length=330, depth=135, tail='round', dorsal='normal',
     marks=both(bars((230, 236, 240, 255), 3), spots((180, 190, 198, 255), 10, 31)), eye=(200, 220, 230))


def ray(name):
    im, d = canvas()
    c = W / 2
    pts = [(c + 120, c - 10), (c, c - 110), (c - 90, c - 10), (c, c + 70)]
    tail = [(c - 80, c), (c - 200, c + 40), (c - 84, c + 12)]
    poly(d, rot(tail, -0.62), (30, 28, 34, 255))
    poly(d, rot(pts, -0.62), (36, 34, 42, 255))
    for e in ((c + 40, c - 30), (c + 40, c + 10)):
        px, py = rot([e], -0.62)[0]
        d.ellipse((px - 9, py - 9, px + 9, py + 9), fill=(255, 150, 60, 255), outline=INK, width=2 * S)
    d.line(rot([(c - 20, c - 60), (c + 60, c - 30)], -0.62), fill=(120, 116, 140, 200), width=5 * S)
    save(im, name, 1.55)


def eel(name):
    im, d = canvas()
    pts = []
    for i in range(60):
        t = i / 59
        x = 70 + t * 380
        y = W / 2 + math.sin(t * math.pi * 2.0) * 60 - (t - 0.5) * 140
        pts.append((x, y))
    for w, col in ((30 * S / 4 * 4, INK), (22 * S / 4 * 4, (64, 28, 20, 255))):
        d.line(pts, fill=col, width=int(w), joint='curve')
    d.line(pts[5:50], fill=(255, 120, 30, 255), width=4 * S, joint='curve')
    hx, hy = pts[-1]
    d.ellipse((hx - 30, hy - 22, hx + 30, hy + 22), fill=(64, 28, 20, 255), outline=INK, width=4 * S)
    d.ellipse((hx + 2, hy - 14, hx + 14, hy - 2), fill=(255, 190, 70, 255), outline=INK, width=2 * S)
    save(im, name)


def halibut(name):
    im, d = canvas()
    c = W / 2
    tail = [(c - 120, c), (c - 175, c - 40), (c - 175, c + 40)]
    poly(d, tail, (120, 104, 84, 255), width=3 * S)
    d.ellipse((c - 130, c - 80, c + 130, c + 80), fill=(148, 128, 100, 255), outline=INK, width=5 * S)
    sp = spots((112, 94, 70, 255), 16, 41, -0.6, 0.6, 5)
    sp(d, c, c, 220, 150, 0.0)
    for e in ((c + 70, c - 22), (c + 82, c + 6)):
        d.ellipse((e[0] - 8, e[1] - 8, e[0] + 8, e[1] + 8), fill=(250, 240, 220), outline=INK, width=3 * S)
    d.line([(c - 80, c - 50), (c + 30, c - 62)], fill=HI, width=4 * S)
    im = im.rotate(32, resample=Image.BICUBIC)
    save(im, name, 1.45)


ray('fish_obsidian_ray')
eel('fish_magma_eel')
halibut('fish_ice_halibut')


def grilled(name):
    fish(name, (138, 82, 44, 255), (176, 112, 62, 255), (110, 62, 32, 255), tail='fork',
         marks=lambda d, cx, cy, L, D, a: [d.line(rot([(cx - L * 0.3 + i * L * 0.14, cy - D * 0.4), (cx - L * 0.22 + i * L * 0.14, cy + D * 0.4)], a), fill=(62, 34, 18, 255), width=6 * S) for i in range(5)],
         eye=(230, 220, 200))


grilled('grilled_fish')


def rod():
    im, d = canvas()
    a = (90, 430)
    b = (440, 70)
    d.line([a, b], fill=INK, width=16 * S)
    d.line([a, b], fill=(150, 98, 52, 255), width=9 * S)
    # grip and spool
    d.line([(a[0], a[1]), (a[0] + 70, a[1] - 72)], fill=INK, width=24 * S)
    d.line([(a[0] + 4, a[1] - 4), (a[0] + 66, a[1] - 68)], fill=(92, 60, 36, 255), width=16 * S)
    sx, sy = a[0] + 104, a[1] - 106
    d.ellipse((sx - 26, sy - 26, sx + 26, sy + 26), fill=(206, 190, 150, 255), outline=INK, width=5 * S)
    d.ellipse((sx - 8, sy - 8, sx + 8, sy + 8), fill=(120, 80, 44, 255), outline=INK, width=3 * S)
    # line and float
    pts = [b] + [(b[0] - 10 + i * 2, b[1] + i * 22 + (i * i) * 0.6) for i in range(1, 10)]
    d.line(pts, fill=(60, 52, 40, 255), width=3 * S, joint='curve')
    fx, fy = pts[-1]
    d.ellipse((fx - 36, fy - 8, fx + 36, fy + 72), fill=(236, 226, 200, 255), outline=INK, width=4 * S)
    d.pieslice((fx - 36, fy - 8, fx + 36, fy + 72), 180, 360, fill=(200, 40, 30, 255), outline=INK, width=4 * S)
    d.line([(150, 340), (300, 190)], fill=HI, width=3 * S)
    save(im, 'fishing_rod')


def strips(name):
    im, d = canvas()
    for i, (dx, dy) in enumerate(((-13, -19), (0, 0), (13, 19))):
        c = (W / 2 + dx * 2.2, W / 2 + dy * 2.2)
        pts = rot([(c[0] - 80, c[1] - 16), (c[0] + 80, c[1] - 21), (c[0] + 74, c[1] + 16), (c[0] - 76, c[1] + 13)], -0.62, c[0], c[1])
        poly(d, pts, (190 - i * 12, 64, 58, 255), width=3 * S)
        d.line(rot([(c[0] - 50, c[1] - 6), (c[0] + 40, c[1] - 9)], -0.62, c[0], c[1]), fill=(240, 170, 160, 220), width=2 * S)
    save(im, name, 1.9)


def lure(name, body, belly, glint):
    im, d = canvas()
    c = W / 2
    pts = rot([(c - 130, c + 6), (c - 40, c - 46), (c + 90, c - 30), (c + 130, c), (c + 90, c + 32), (c - 40, c + 40)], -0.62)
    poly(d, pts, body)
    lower = rot([(c - 120, c + 10), (c + 120, c + 4), (c + 90, c + 30), (c - 40, c + 38)], -0.62)
    d.polygon(lower, fill=belly)
    d.line(pts + [pts[0]], fill=INK, width=5 * S, joint='curve')
    ex, ey = rot([(c + 70, c - 8)], -0.62)[0]
    d.ellipse((ex - 9, ey - 9, ex + 9, ey + 9), fill=glint, outline=INK, width=3 * S)
    # the hook under it
    hx, hy = rot([(c - 20, c + 40)], -0.62)[0]
    d.arc((hx - 34, hy, hx + 20, hy + 70), 20, 250, fill=INK, width=9 * S)
    d.arc((hx - 34, hy, hx + 20, hy + 70), 20, 250, fill=(186, 186, 190, 255), width=4 * S)
    d.line(rot([(c - 60, c - 26), (c + 50, c - 22)], -0.62), fill=HI, width=4 * S)
    save(im, name, 1.6)


rod()
strips('cut_bait')
lure('ember_lure', (214, 92, 30, 255), (250, 190, 70, 255), (255, 230, 120, 255))
lure('rime_lure', (120, 170, 214, 255), (224, 240, 250, 255), (250, 255, 255, 255))
print('icons written')
