"""The journal's map picture (Data/UI/map.png) over every land on L_Island: the Old Island's picture kept pixel for pixel,
the Swamp east of Storm peaks (its zones and water looks), Storm peaks drawn north of it from its heightmap in the same style (navy sea, a pale shore glow, shaded relief tinted by
the land, contours every 50 m), and the Ancient forest south of it (Tools/terrain/forest.mjs: the same relief, tinted by
its sub-zones, its rivers in blue). 4 m per pixel; the square's world corner and size go to tuning ui.mapMin / ui.mapSize
(WRUIMap reads them). Run after Tools/terrain/storm_peaks.mjs:  python -I Tools/terrain/world_map.py
(The island-only picture is kept as Art/Terrain/island_map.png the first time this runs.)"""
import json
import os
import struct

from PIL import Image, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
UI_MAP = os.path.join(ROOT, 'Data', 'UI', 'map.png')
ISLAND_MAP = os.path.join(ROOT, 'Art', 'Terrain', 'island_map.png')
PX = 4.0                          # metres per pixel
ISL_MIN = -2018.0                 # the island picture's world edge (m): pixel centre 0.5 = -2016 m
if not os.path.exists(ISLAND_MAP):
    Image.open(UI_MAP).save(ISLAND_MAP)
island = Image.open(ISLAND_MAP).convert('RGBA')
storm = json.load(open(os.path.join(ROOT, 'Data', 'storm_peaks.json'), encoding='utf-8'))
F = storm['frame']
raw = open(os.path.join(ROOT, 'Art', 'Terrain', 'storm_height.r16'), 'rb').read()
N = F['samples']


def h(x, y):
    i = int(round((x - F['x0']) / F['step'])); j = int(round((y - F['y0']) / F['step']))
    if i < 0 or j < 0 or i >= N or j >= N:
        return None
    return (struct.unpack_from('<H', raw, (j * N + i) * 2)[0] - 32768) / F['unitsPerMetre']


# The Swamp (Tools/terrain/swamp.mjs) east of Storm peaks: the square grows east to its land.
SWP = None
try:
    SWP = json.load(open(os.path.join(ROOT, 'Data', 'swamp.json'), encoding='utf-8'))
    SWRAW = open(os.path.join(ROOT, 'Art', 'Terrain', 'swamp_height.r16'), 'rb').read()
except OSError:
    SWP = None


def swamp_zone(x, y):
    Z = SWP['zones']
    i = int((x - Z['x0']) // Z['cell']); j = int((y - Z['y0']) // Z['cell'])
    if i < 0 or j < 0 or i >= Z['width'] or j >= Z['height']:
        return '~'
    return Z['rows'][j][i]


def swamp_water(x, y):
    Wd = SWP['water']
    i = int((x - Wd['x0']) // Wd['cell']); j = int((y - Wd['y0']) // Wd['cell'])
    if i < 0 or j < 0 or i >= Wd['width'] or j >= Wd['height']:
        return '.'
    return Wd['rows'][j][i]


def swamp_h(x, y):
    G = SWP['frame']
    i = int(round((x - G['x0']) / G['step'])); j = int(round((y - G['y0']) / G['step']))
    if i < 0 or j < 0 or i >= G['samples'] or j >= G['samples']:
        return None
    return (struct.unpack_from('<H', SWRAW, (j * G['samples'] + i) * 2)[0] - 32768) / G['unitsPerMetre']


K = int(round((ISL_MIN - (F['y0'] - 22)) / PX))    # pixels added north and west so the island keeps its grid
MIN = ISL_MIN - K * PX
SIZE = K + island.width
if SWP:
    east = MIN
    Z = SWP['zones']
    for j, r in enumerate(Z['rows']):
        for i in range(len(r) - 1, -1, -1):
            if r[i] != '~':
                east = max(east, Z['x0'] + (i + 1) * Z['cell'] + 80)
                break
    SIZE = max(SIZE, int((east - MIN) / PX) + 1)
# The Ancient forest (Tools/terrain/forest.mjs) south of the island: the square grows south (and so east, it stays square)
# to its south edge and a margin of sea; the corner stays put, so every pixel already drawn keeps its place.
FOR = None
try:
    FOR = json.load(open(os.path.join(ROOT, 'Data', 'forest.json'), encoding='utf-8'))
    FORRAW = open(os.path.join(ROOT, 'Art', 'Terrain', 'forest_height.r16'), 'rb').read()
except OSError:
    FOR = None
if FOR:
    FF = FOR['frame']
    SIZE = max(SIZE, int((FF['y0'] + (FF['samples'] - 1) * FF['step'] + 40 - MIN) / PX) + 1)
SEA = (24, 36, 46)
out = Image.new('RGBA', (SIZE, SIZE), SEA + (255,))
H = [[None] * SIZE for _ in range(SIZE)]
for py in range(SIZE):
    y = MIN + (py + 0.5) * PX
    if y >= ISL_MIN:
        continue
    for px in range(SIZE):
        H[py][px] = h(MIN + (px + 0.5) * PX, y)
pix = out.load()
# The Swamp first (its zones decide; the island picture is pasted over it last, so the neck shows only on the sea).
SW_TINT = {'1': (150, 132, 84), '2': (58, 82, 58), '3': (84, 90, 74), '4': (92, 98, 76), '5': (196, 188, 168), 'f': (196, 206, 214)}
SW_WATER = {'m': (26, 30, 32), 'd': (110, 140, 60), 'b': (98, 30, 26), 'k': (40, 58, 64), 'q': (104, 100, 92), 'g': (90, 104, 120)}
if SWP:
    for py in range(1, SIZE - 1):
        y = MIN + (py + 0.5) * PX
        for px in range(1, SIZE - 1):
            x = MIN + (px + 0.5) * PX
            z = swamp_zone(x, y)
            if z == '~':
                continue
            wk = swamp_water(x, y)
            v = swamp_h(x, y)
            if v is None:
                continue
            if wk in SW_WATER and wk != 'q' and wk != 'g':
                pix[px, py] = SW_WATER[wk] + (255,)
                continue
            dzdx = ((swamp_h(x + PX, y) or v) - (swamp_h(x - PX, y) or v)) / (2 * PX)
            dzdy = ((swamp_h(x, y + PX) or v) - (swamp_h(x, y - PX) or v)) / (2 * PX)
            shade = max(0.5, min(1.25, 0.95 + (-dzdx * 3 - dzdy * 3) * 0.8))
            base = SW_WATER['q'] if wk == 'q' else SW_TINT.get(z, (90, 96, 80))
            pix[px, py] = tuple(min(255, int(ch * shade)) for ch in base) + (255,)
for py in range(1, SIZE - 1):
    y = MIN + (py + 0.5) * PX
    if y >= ISL_MIN:
        continue
    for px in range(1, SIZE - 1):
        v = H[py][px]
        if v is None or v < 1.0:
            continue
        if SWP and pix[px, py][:3] != SEA and swamp_zone(MIN + (px + 0.5) * PX, y) != '~':
            continue
        # Hillshade from the north-west, as the island's picture.
        dzdx = ((H[py][px + 1] or 0) - (H[py][px - 1] or 0)) / (2 * PX)
        dzdy = ((H[py + 1][px] or 0) - (H[py - 1][px] or 0)) / (2 * PX)
        shade = max(0.35, min(1.3, 0.95 + (-dzdx * 0.6 - dzdy * 0.6) * 0.8))
        # Storm peaks' tint: grey-green low, slate up the cliffs, pale granite high, a violet cast on the Crown.
        t = min(1.0, v / 900.0)
        base = (int(84 + 44 * t), int(96 + 34 * t), int(92 + 58 * t))
        if v > 1100:
            base = (170, 168, 190)
        c = tuple(min(255, int(ch * shade)) for ch in base)
        # Contours every 50 m (every 250 m a heavier one).
        lo = min(H[py][px - 1] or v, H[py - 1][px] or v)
        if int(lo // 50) != int(v // 50):
            k = 0.78 if int(v // 250) != int(lo // 250) else 0.9
            c = tuple(int(ch * k) for ch in c)
        pix[px, py] = c + (255,)
# A pale glow along the new coast (the island's picture has one).
land = Image.new('L', (SIZE, SIZE), 0)
lp = land.load()
for py in range(SIZE):
    for px in range(SIZE):
        if pix[px, py][:3] != SEA:
            lp[px, py] = 255
glow = land.filter(ImageFilter.GaussianBlur(5))
gp = glow.load()
for py in range(SIZE):
    y = MIN + (py + 0.5) * PX
    if y >= ISL_MIN:
        continue
    for px in range(SIZE):
        if pix[px, py][:3] == SEA and gp[px, py] > 0:
            a = gp[px, py] / 255.0 * 0.35
            pix[px, py] = tuple(int(SEA[i] + (120 - SEA[i]) * a) for i in range(3)) + (255,)
out.paste(island, (K, K))
# The Swamp's frozen-bog neck runs on over the island picture's sea to the tundra shore: draw it back over that sea.
if SWP:
    ISLRAW = open(os.path.join(ROOT, 'Art', 'Terrain', 'island_height.r16'), 'rb').read()

    def island_h(x, y):
        i = int(round(x + 2016)); j = int(round(y + 2016))
        if i < 0 or j < 0 or i >= 4033 or j >= 4033:
            return None
        return (struct.unpack_from('<H', ISLRAW, (j * 4033 + i) * 2)[0] - 32768) / 32

    for py in range(K, SIZE - 1):
        y = MIN + (py + 0.5) * PX
        for px in range(1, SIZE - 1):
            x = MIN + (px + 0.5) * PX
            if swamp_zone(x, y) == '~':
                continue
            v, ih = swamp_h(x, y), island_h(x, y)
            if v is None or v < 0.6 or (ih is not None and ih > v):
                continue
            pix[px, py] = SW_TINT['f'] + (255,)
# The Ancient forest: everything south of the island picture, and its neck where it rises out of the island picture's sea.
if FOR:
    import numpy as np
    from PIL import ImageDraw
    FF = FOR['frame']; FZ = FOR['zones']; FN = FF['samples']
    fh = (np.frombuffer(FORRAW, dtype='<u2').reshape(FN, FN).astype(np.float32) - 32768) / FF['unitsPerMetre']
    ih = (np.frombuffer(open(os.path.join(ROOT, 'Art', 'Terrain', 'island_height.r16'), 'rb').read(), dtype='<u2')
          .reshape(4033, 4033).astype(np.float32) - 32768) / 32
    # The pixels the forest's frame covers (one pixel of border for the slopes).
    p0x = max(0, int((FF['x0'] - MIN) / PX) - 1); p1x = min(SIZE, int((FF['x0'] + (FN - 1) * FF['step'] - MIN) / PX) + 2)
    p0y = max(0, int((FF['y0'] - MIN) / PX) - 1); p1y = min(SIZE, int((FF['y0'] + (FN - 1) * FF['step'] - MIN) / PX) + 2)
    X, Y = np.meshgrid(MIN + (np.arange(p0x, p1x) + 0.5) * PX, MIN + (np.arange(p0y, p1y) + 0.5) * PX)
    fi = np.rint((X - FF['x0']) / FF['step']).astype(int); fj = np.rint((Y - FF['y0']) / FF['step']).astype(int)
    inside = (fi >= 0) & (fj >= 0) & (fi < FN) & (fj < FN)
    V = np.where(inside, fh[np.clip(fj, 0, FN - 1), np.clip(fi, 0, FN - 1)], 0.0)
    ii = np.rint(X + 2016).astype(int); ij = np.rint(Y + 2016).astype(int)
    IH = np.where((ii >= 0) & (ij >= 0) & (ii < 4033) & (ij < 4033), ih[np.clip(ij, 0, 4032), np.clip(ii, 0, 4032)], -50.0)
    landF = inside & (V >= 1.0)

    def nb(a, dy, dx):   # each pixel's neighbour (the pixel itself at the edge)
        b = np.pad(a, 1, mode='edge')
        return b[1 + dy:1 + dy + a.shape[0], 1 + dx:1 + dx + a.shape[1]]
    # Hillshade from the north-west, as Storm peaks'.
    dzdx = (nb(V, 0, 1) - nb(V, 0, -1)) / (2 * PX)
    dzdy = (nb(V, 1, 0) - nb(V, -1, 0)) / (2 * PX)
    shade = np.clip(0.95 + (-dzdx * 0.6 - dzdy * 0.6) * 0.8, 0.35, 1.3)
    # The sub-zone tints ('h', the Hollow Root, drawn as the bark woods around it; land on a sea cell as meadow).
    FO_TINT = {'1': (127, 163, 74), '2': (78, 122, 58), '3': (94, 140, 49), '4': (88, 78, 52), '5': (58, 82, 34), 's': (96, 92, 88), 'h': (88, 78, 52)}
    zi = np.floor((X - FZ['x0']) / FZ['cell']).astype(int); zj = np.floor((Y - FZ['y0']) / FZ['cell']).astype(int)
    zgrid = np.array([[ord(c) for c in r] for r in FZ['rows']], dtype=np.int32)
    zl = np.where((zi >= 0) & (zj >= 0) & (zi < FZ['width']) & (zj < FZ['height']),
                  zgrid[np.clip(zj, 0, FZ['height'] - 1), np.clip(zi, 0, FZ['width'] - 1)], ord('~'))
    base = np.zeros(X.shape + (3,), np.float32) + np.array(FO_TINT['1'], np.float32)
    for k, col in FO_TINT.items():
        base[zl == ord(k)] = col
    rgb = np.minimum(255, np.floor(base * shade[..., None]))
    # Contours every 50 m (every 250 m a heavier one).
    lo = np.minimum(nb(V, 0, -1), nb(V, -1, 0))
    lo = np.where(lo < 1.0, V, lo)
    cross = np.floor(lo / 50) != np.floor(V / 50)
    heavy = np.floor(lo / 250) != np.floor(V / 250)
    rgb = np.where((cross & heavy)[..., None], np.floor(rgb * 0.78), np.where(cross[..., None], np.floor(rgb * 0.9), rgb))
    # The rivers in blue at their width (the Hollow Root's underground pool isn't on the map).
    rl = Image.new('L', (p1x - p0x, p1y - p0y), 0)
    rd = ImageDraw.Draw(rl)
    for r in FOR['rivers']:
        if r['kind'] == 'pool':
            continue
        pts = [((q[0] - MIN) / PX - 0.5 - p0x, (q[1] - MIN) / PX - 0.5 - p0y) for q in r['pts']]
        for k in range(len(pts) - 1):
            rd.line([pts[k], pts[k + 1]], fill=255, width=max(1, int(round(r['widths'][k] / PX))))
    rgb[(np.array(rl) > 0) & landF] = (70, 120, 160)
    # A pale glow along the new coast, as Storm peaks'.
    gl = np.array(Image.fromarray((landF * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(5)), np.float32) / 255.0 * 0.35
    sea = np.array(SEA, np.float32)
    final = np.where(landF[..., None], rgb, np.floor(sea + (120 - sea) * gl[..., None]))
    PY, PXa = np.meshgrid(np.arange(p0y, p1y), np.arange(p0x, p1x), indexing='ij')
    south = PY >= K + island.height                     # below the island picture: all the forest's
    neck = ~south & landF & (V > IH)                    # over the island picture's sea, where the forest is the higher
    put = (south | neck) & (PY >= 1) & (PY < SIZE - 1) & (PXa >= 1) & (PXa < SIZE - 1)
    cur = np.array(out.crop((p0x, p0y, p1x, p1y)).convert('RGB'), np.float32)
    cur[put] = final[put]
    out.paste(Image.fromarray(cur.astype(np.uint8), 'RGB').convert('RGBA'), (p0x, p0y))
out.save(UI_MAP, optimize=True)
print(f'map.png {SIZE} px from {MIN:.0f} m: ui.mapMin {MIN * 100:.0f}, ui.mapSize {SIZE * PX * 100:.0f}')
