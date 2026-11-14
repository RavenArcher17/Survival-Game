"""Item icons for the world systems (Data/UI/Icons/*.png, 128 px; cooks/build-world-systems.md), in the UI's flat style
of make_farm_icons.py: thick dark outline, flat fills, one pale highlight, drawn at 4x and scaled down.
The Scratch-stone (a flat stone with cut lines and the flint that cuts them), Cinder salve (a clay pot of glowing
salve), the Obsidian arrow (a knapped black glass head), Spiked boots (a hide boot with iron spikes under it) and the
eight effigies (a bound figure of reed and bone carrying its boss's mark in the boss's colour).
Run: python Tools/art/make_world_icons.py  (only writes icons that are missing; --all redraws them)."""
import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_farm_icons as FI  # noqa: E402

S, INK, HI = FI.S, FI.INK, FI.HI
P, rgb, shade, poly, ell, canvas, done, LW = FI.P, FI.rgb, FI.shade, FI.poly, FI.ell, FI.canvas, FI.done, FI.LW


def scratch_stone():
    im, d = canvas()
    poly(d, [(18, 70), (30, 44), (66, 34), (104, 46), (112, 74), (92, 96), (46, 100), (22, 90)], rgb((128, 122, 112)))
    for y, x0, x1 in ((54, 40, 86), (64, 36, 94), (74, 42, 80), (84, 50, 88)):
        d.line([P(x0, y), P(x1, y + 2)], fill=INK, width=3 * S)
    d.line([P(34, 50), P(62, 42)], fill=HI, width=3 * S)
    # The flint that cuts: a dark shard lying across the corner.
    poly(d, [(82, 18), (104, 10), (112, 26), (94, 36)], rgb((52, 48, 58)))
    d.line([P(88, 20), P(102, 15)], fill=HI, width=2 * S)
    done(im, 'scratch_stone')


def cinder_salve():
    im, d = canvas()
    poly(d, [(30, 54), (98, 54), (92, 106), (36, 106)], rgb((150, 86, 52)))
    ell(d, 64 * S, 54 * S, 36 * S, 11 * S, rgb((235, 110, 30)))
    ell(d, 64 * S, 54 * S, 20 * S, 5 * S, rgb((255, 196, 90)), width=2 * S)
    d.line([P(36, 74), P(92, 74)], fill=shade((150, 86, 52), 0.6), width=3 * S)
    d.line([P(40, 62), P(40, 98)], fill=HI, width=3 * S)
    # A sulfur crystal and an ember beside it: what's in it.
    poly(d, [(98, 28), (108, 20), (116, 32), (106, 40)], rgb((222, 200, 70)))
    done(im, 'cinder_salve')


def obsidian_arrow():
    im, d = canvas()
    d.line([P(20, 108), P(88, 40)], fill=INK, width=9 * S)
    d.line([P(20, 108), P(88, 40)], fill=rgb((140, 100, 60)), width=5 * S)
    for k in (0, 1):
        o = k * 10
        poly(d, [(22 + o, 98 - o), (14 + o, 92 - o), (26 + o, 88 - o)], rgb((210, 210, 200)), width=3 * S)
        poly(d, [(30 + o, 106 - o), (36 + o, 114 - o), (40 + o, 102 - o)], rgb((210, 210, 200)), width=3 * S)
    poly(d, [(84, 32), (112, 16), (96, 44)], rgb((26, 22, 34)))
    d.line([P(92, 30), P(104, 24)], fill=rgb((150, 120, 200)), width=2 * S)
    d.line([P(80, 44), P(88, 36)], fill=rgb((170, 150, 120)), width=4 * S)
    done(im, 'obsidian_arrow')


def spiked_boots():
    im, d = canvas()
    hide = (130, 88, 54)
    poly(d, [(40, 16), (72, 16), (74, 72), (108, 82), (110, 100), (36, 100)], rgb(hide))
    d.line([P(40, 30), P(72, 30)], fill=shade(hide, 0.6), width=4 * S)
    for y in (42, 54, 66):
        d.line([P(46, y), P(66, y + 4)], fill=rgb((200, 180, 140)), width=2 * S)
    poly(d, [(34, 100), (112, 100), (112, 108), (34, 108)], rgb((70, 66, 64)))
    for x in range(40, 112, 12):
        poly(d, [(x, 108), (x + 8, 108), (x + 4, 120)], rgb((150, 150, 158)), width=3 * S)
    d.line([P(44, 20), P(44, 92)], fill=HI, width=3 * S)
    done(im, 'spiked_boots')


def effigy(name, colour, mark):
    """A reed-bound figure on a bone stake, a cloth band in the boss's colour, its mark on the chest."""
    im, d = canvas()
    reed = (176, 150, 96)
    d.line([P(64, 120), P(64, 92)], fill=INK, width=9 * S)
    d.line([P(64, 120), P(64, 92)], fill=rgb((226, 214, 190)), width=5 * S)
    poly(d, [(46, 40), (82, 40), (88, 96), (40, 96)], rgb(reed))
    ell(d, 64 * S, 28 * S, 15 * S, 15 * S, rgb(reed))
    for y in (52, 70, 86):
        d.line([P(42, y), P(86, y)], fill=shade(reed, 0.55), width=3 * S)
    d.line([P(46, 46), P(22, 62)], fill=INK, width=8 * S)
    d.line([P(82, 46), P(106, 62)], fill=INK, width=8 * S)
    d.line([P(46, 46), P(22, 62)], fill=rgb(reed), width=4 * S)
    d.line([P(82, 46), P(106, 62)], fill=rgb(reed), width=4 * S)
    poly(d, [(44, 58), (84, 58), (85, 66), (43, 66)], rgb(colour), width=3 * S)
    c = rgb(colour)
    if mark == 'horn':
        poly(d, [(52, 22), (40, 4), (56, 16)], c, width=3 * S)
        poly(d, [(76, 22), (88, 4), (72, 16)], c, width=3 * S)
    elif mark == 'crown':
        poly(d, [(50, 18), (54, 6), (60, 14), (64, 4), (68, 14), (74, 6), (78, 18)], c, width=3 * S)
    elif mark == 'heart':
        poly(d, [(64, 92), (52, 78), (56, 70), (64, 76), (72, 70), (76, 78)], c, width=3 * S)
    elif mark == 'tusk':
        d.arc((30 * S, 70 * S, 60 * S, 104 * S), 90, 220, fill=INK, width=7 * S)
        d.arc((68 * S, 70 * S, 98 * S, 104 * S), -40, 90, fill=INK, width=7 * S)
        d.arc((30 * S, 70 * S, 60 * S, 104 * S), 90, 220, fill=c, width=4 * S)
        d.arc((68 * S, 70 * S, 98 * S, 104 * S), -40, 90, fill=c, width=4 * S)
    elif mark == 'fang':
        poly(d, [(56, 74), (64, 94), (72, 74)], c, width=3 * S)
    elif mark == 'flame':
        poly(d, [(64, 70), (54, 86), (58, 94), (70, 94), (74, 86)], c, width=3 * S)
    elif mark == 'scale':
        for k in range(3):
            ell(d, (52 + k * 12) * S, 82 * S, 6 * S, 6 * S, c, width=3 * S)
    elif mark == 'club':
        d.line([P(96, 40), P(110, 96)], fill=INK, width=10 * S)
        d.line([P(96, 40), P(110, 96)], fill=c, width=6 * S)
    d.line([P(52, 22), P(58, 18)], fill=HI, width=3 * S)
    done(im, name)


def main():
    scratch_stone()
    cinder_salve()
    obsidian_arrow()
    spiked_boots()
    effigy('effigy_vulkarn', (232, 96, 34), 'flame')
    effigy('effigy_skaldrith', (120, 186, 236), 'scale')
    effigy('effigy_obsidian_warden', (60, 54, 70), 'heart')
    effigy('effigy_ignessa', (250, 170, 70), 'crown')
    effigy('effigy_cinderhorn', (200, 60, 30), 'horn')
    effigy('effigy_grimjaw', (110, 150, 120), 'club')
    effigy('effigy_pale_hunter', (220, 228, 236), 'fang')
    effigy('effigy_old_tusker', (236, 226, 200), 'tusk')


if __name__ == '__main__':
    main()
