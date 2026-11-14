"""Item icons for machinery (Data/UI/Icons/*.png, 128 px; cooks/build-machinery.md), in the UI's flat style of
make_farm_icons.py (thick dark outline, flat fills, one pale highlight, drawn at 4x): planks, flour, bone, bone meal,
stone dust and gears. Run: python Tools/art/make_machine_icons.py  (only writes icons that are missing; --all redraws)."""
import math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_farm_icons as FI

S, INK, HI, LW, P = FI.S, FI.INK, FI.HI, FI.LW, FI.P
rgb, shade, poly, done, canvas, ell = FI.rgb, FI.shade, FI.poly, FI.done, FI.canvas, FI.ell


def planks(name):
    # Three sawn boards stacked crosswise, their end grain showing, the top one's grain lines.
    im, d = canvas()
    cols = [(196, 150, 96), (182, 136, 84), (206, 162, 108)]
    for k, (y, c) in enumerate(zip((88, 70, 52), cols)):
        poly(d, [(20, y), (92, y - 26), (110, y - 18), (38, y + 8)], rgb(c))
        poly(d, [(20, y), (38, y + 8), (38, y + 18), (20, y + 10)], rgb(shade(c, 0.75)))
        poly(d, [(38, y + 8), (110, y - 18), (110, y - 8), (38, y + 18)], rgb(shade(c, 0.88)))
    for k in range(3):
        d.line([P(40 + k * 14, 52 - 4 - k * 2), P(80 + k * 6, 38 - k * 2)], fill=rgb((150, 105, 60)), width=2 * S)
    d.line([P(30, 50), P(70, 34)], fill=HI, width=3 * S)
    done(im, name)


def sack(name, col, spill):
    # A tied grain sack with its powder spilled at its foot.
    im, d = canvas()
    d.chord((14 * S, 92 * S, 116 * S, 124 * S), 180, 360, fill=rgb(spill), outline=INK, width=4 * S)
    poly(d, [(36, 46), (92, 46), (104, 104), (26, 104)], rgb(col))
    poly(d, [(44, 30), (84, 30), (92, 46), (36, 46)], rgb(shade(col, 0.9)))
    d.line([P(40, 44), P(88, 44)], fill=rgb((120, 80, 40)), width=5 * S)
    d.line([P(36, 60), P(32, 96)], fill=HI, width=4 * S)
    done(im, name)


def gears(name):
    # Two bronze cogs meshing, iron hubs.
    im, d = canvas()

    def cog(cx, cy, r, teeth, phase, col):
        pts = []
        for k in range(teeth * 4):
            a = phase + k * math.pi * 2 / (teeth * 4)
            rr = r + (8 if (k % 4) in (1, 2) else 0)
            pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
        poly(d, pts, rgb(col), 4 * S)
        ell(d, cx * S, cy * S, r * 0.42 * S, r * 0.42 * S, rgb(shade(col, 0.8)), width=3 * S)
        ell(d, cx * S, cy * S, r * 0.18 * S, r * 0.18 * S, rgb((50, 48, 46)), width=3 * S)
        for k in range(4):
            a = phase + k * math.pi / 2
            d.line([P(cx + r * 0.45 * math.cos(a), cy + r * 0.45 * math.sin(a)), P(cx + r * 0.85 * math.cos(a), cy + r * 0.85 * math.sin(a))], fill=rgb(shade(col, 0.7)), width=4 * S)

    cog(50, 72, 30, 10, 0.1, (184, 128, 58))
    cog(92, 40, 20, 7, 0.4, (200, 146, 70))
    d.arc((26 * S, 48 * S, 74 * S, 96 * S), 200, 250, fill=HI, width=4 * S)
    done(im, name)


def main():
    planks('planks')
    sack('flour', (214, 196, 160), (246, 242, 232))
    FI.bones('bone')
    FI.heap('bone_meal', (228, 218, 194))
    FI.heap('stone_dust', (150, 148, 142), crystals=True)
    gears('gears')


if __name__ == '__main__':
    main()
    print('machine icons written to', os.path.normpath(FI.OUT))
