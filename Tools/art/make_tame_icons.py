"""Item icons for taming (Data/UI/Icons/*.png, 128 px; cooks/build-taming.md), in the UI's flat style of
make_farm_icons.py: thick dark outline, flat fills, one pale highlight, drawn at 4x and scaled down.
Riding saddles (side view: blanket, seat between pommel and cantle, skirt, stirrup; the tier's metal on pommel, cantle
and rivets), the pack saddle (a sawbuck X over two panniers), barding (a chest plate with its neck lames, the set's band
in its colour), fertilised eggs (in the species' colour, speckled, a pale highlight) and rope.
Run: python Tools/art/make_tame_icons.py  (only writes icons that are missing; --all redraws them)."""
import json, math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_farm_icons as FI  # noqa: E402  (its helpers; its main() only runs as a script)

S, INK, HI = FI.S, FI.INK, FI.HI
P, rgb, shade, poly, ell, canvas, done, LW = FI.P, FI.rgb, FI.shade, FI.poly, FI.ell, FI.canvas, FI.done, FI.LW
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')


def rivets(d, pts, col):
    for x, y in pts:
        ell(d, x * S, y * S, 2.6 * S, 2.6 * S, rgb(col), width=2 * S)


def saddle(name, leather, blanket, metal=None):
    im, d = canvas()
    # The blanket under it, its bound edge.
    poly(d, [(20, 58), (108, 58), (104, 80), (24, 80)], rgb(blanket))
    d.line([P(24, 76), P(104, 76)], fill=shade(blanket, 0.55), width=3 * S)
    # The seat: a dip between the pommel (front, right) and the taller cantle (back, left).
    seat = [(26, 60), (30, 40), (38, 34), (46, 44), (64, 50), (82, 46), (92, 38), (100, 42), (102, 60)]
    poly(d, seat, rgb(leather))
    d.line([P(40, 50), P(64, 56), P(88, 50)], fill=HI, width=3 * S)
    # Skirt and fender down the side, the stirrup leather and the stirrup.
    poly(d, [(44, 60), (86, 60), (82, 92), (50, 92)], shade(leather, 0.82))
    d.line([P(66, 62), P(66, 98)], fill=INK, width=4 * S)
    poly(d, [(56, 98), (76, 98), (74, 112), (58, 112)], rgb(metal if metal else (80, 70, 60)))
    d.line([P(60, 106), P(72, 106)], fill=INK, width=3 * S)
    if metal:
        # Metal on the cantle and pommel, rivets down the skirt.
        poly(d, [(30, 42), (38, 34), (42, 40), (34, 46)], rgb(metal))
        poly(d, [(90, 40), (96, 36), (101, 42), (95, 46)], rgb(metal))
        rivets(d, [(50, 66), (50, 76), (50, 86), (80, 66), (80, 76), (80, 86)], metal)
    done(im, name)


def pack_saddle(name):
    im, d = canvas()
    wood, hide, rope = (110, 70, 38), (130, 86, 50), (170, 145, 100)
    # Panniers either side of a pad.
    poly(d, [(14, 64), (46, 64), (44, 108), (16, 108)], rgb(hide))
    poly(d, [(82, 64), (114, 64), (112, 108), (84, 108)], rgb(hide))
    for x0 in (16, 84):
        for k in range(5):
            d.line([P(x0 + k * 6, 66), P(x0 + k * 6 + 3, 72)], fill=rgb(rope), width=2 * S)
    poly(d, [(40, 58), (88, 58), (86, 70), (42, 70)], rgb((150, 140, 110)))
    # The sawbuck: two bars crossing above.
    d.line([P(40, 66), P(76, 24)], fill=INK, width=9 * S)
    d.line([P(88, 66), P(52, 24)], fill=INK, width=9 * S)
    d.line([P(40, 66), P(76, 24)], fill=rgb(wood), width=5 * S)
    d.line([P(88, 66), P(52, 24)], fill=rgb(wood), width=5 * S)
    ell(d, 64 * S, 39 * S, 5 * S, 5 * S, rgb(rope), width=2 * S)
    done(im, name)


def barding(name, metal, band=None):
    im, d = canvas()
    # The peytral: a curved chest plate; above it three neck lames.
    poly(d, [(22, 56), (64, 66), (106, 56), (100, 98), (64, 112), (28, 98)], rgb(metal))
    d.line([P(30, 70), P(64, 80), P(98, 70)], fill=HI, width=3 * S)
    for k, (y, w) in enumerate([(46, 30), (34, 24), (22, 18)]):
        poly(d, [(64 - w, y + 8), (64, y), (64 + w, y + 8), (64 + w - 4, y + 14), (64, y + 8), (64 - w + 4, y + 14)], shade(metal, 0.92 - k * 0.05))
    if band:
        d.line([P(30, 96), P(64, 108), P(98, 96)], fill=rgb(band), width=5 * S)
    rivets(d, [(34, 64), (94, 64), (40, 92), (88, 92)], shade(metal, 0.7)[:3])
    done(im, name)


def egg(name, col):
    im, d = canvas()
    shell = [min(255, int(c * 0.5 + 0.5 * 215)) for c in col]
    pts = []
    for i in range(40):
        a = 2 * math.pi * i / 40
        r = 30 if math.sin(a) > 0 else 30 + 8 * -math.sin(a) * 0.5
        pts.append((64 + math.cos(a) * 30, 70 + math.sin(a) * (40 if math.sin(a) > 0 else 46)))
    poly(d, pts, rgb(shell))
    for k in range(14):
        a, r = k * 2.3, 6 + (k * 7) % 22
        x, y = 64 + math.cos(a) * r * 0.9, 74 + math.sin(a) * r * 1.2
        ell(d, x * S, y * S, 2.4 * S, 2.0 * S, shade(shell, 0.55), outline=None, width=0)
    d.arc((44 * S, 36 * S, 70 * S, 70 * S), 200, 250, fill=HI, width=4 * S)
    done(im, name)


def rope(name):
    im, d = canvas()
    col = (176, 148, 98)
    for k in range(4):
        r = 34 - k * 7
        d.ellipse(((64 - r) * S, (64 - r * 0.8) * S, (64 + r) * S, (64 + r * 0.8) * S), outline=INK, width=9 * S)
        d.ellipse(((64 - r) * S, (64 - r * 0.8) * S, (64 + r) * S, (64 + r * 0.8) * S), outline=rgb(col), width=5 * S)
    d.line([P(94, 70), P(110, 100)], fill=INK, width=9 * S)
    d.line([P(94, 70), P(110, 100)], fill=rgb(col), width=5 * S)
    done(im, name)


def main():
    saddle('saddle_hide', (120, 78, 40), (150, 132, 100))
    saddle('saddle_bronze', (88, 48, 22), (40, 56, 104), (200, 140, 60))
    saddle('saddle_iron', (82, 44, 20), (110, 26, 22), (96, 98, 104))
    saddle('saddle_steel', (34, 24, 18), (34, 38, 50), (186, 192, 204))
    pack_saddle('saddle_pack')
    barding('barding_iron', (90, 92, 98))
    barding('barding_steel', (176, 182, 194))
    bands = {'forgeplate': (230, 80, 20), 'rimeguard': (150, 210, 245), 'stormweave': (80, 120, 230), 'dunewrap': (220, 170, 80),
             'heartguard': (60, 120, 45), 'skysilk': (225, 230, 245), 'wardcloth': (150, 85, 200), 'mirehide': (100, 28, 24),
             'reefmail': (30, 150, 150), 'gravecloth': (100, 100, 92), 'sporeweave': (130, 160, 70), 'hollowplate': (85, 35, 120)}
    for sid, band in bands.items():
        barding('barding_' + sid, (40, 38, 36) if sid == 'forgeplate' else (170, 176, 188), band)
    creatures = json.load(open(os.path.join(ROOT, 'Data', 'creatures.json'), encoding='utf-8'))['creatures']
    colours = {c['id']: c.get('color', [0.7, 0.65, 0.55]) for c in creatures if 'id' in c}
    for cid in ['magmaw', 'scorchwing', 'basaltback', 'kilnmite', 'glowmoth', 'glacimaw', 'rimeshell', 'snowpuff', 'windmane']:
        c = colours.get(cid, [0.7, 0.65, 0.55])
        egg('fertile_' + cid, [v * 255 for v in c])
    rope('rope')


if __name__ == '__main__':
    main()
