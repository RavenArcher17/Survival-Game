"""Item icons for player raiding (Data/UI/Icons/*.png, 128 px; cooks/build-server-raids.md 2.3), in the UI's flat style
of make_farm_icons.py: thick dark outline, flat fills, one pale highlight, drawn at 4x and scaled down.
Saltpetre (white feathery nitre crystals on dark cave rock), clay (a wet grey-brown lump with a scoop mark), black powder
(a paper twist spilling dark grains), the fire pot (a terracotta jar with a cloth bung and a lit rag wick), the powder
charge (the petard: a bronze bell on an oak plank with a coil of slow match), the mining charge (a hide cartridge with a
clay plug and a fuse), the siege ladder (ash rails, rungs, iron hooks) and the code breaker (a resonant-crystal fork in a
bronze frame).
Run: python Tools/art/make_raid_icons.py  (only writes icons that are missing; --all redraws them)."""
import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_farm_icons as FI  # noqa: E402

S, INK, HI = FI.S, FI.INK, FI.HI
P, rgb, shade, poly, ell, canvas, done, LW = FI.P, FI.rgb, FI.shade, FI.poly, FI.ell, FI.canvas, FI.done, FI.LW

BRONZE = (176, 120, 58)
OAK = (112, 74, 42)
IRON = (84, 86, 92)
MATCH = (196, 172, 120)


def saltpetre():
    im, d = canvas()
    poly(d, [(14, 96), (28, 66), (60, 56), (98, 62), (116, 92), (100, 112), (30, 112)], rgb((62, 58, 60)))
    # Feathery white needles growing out of the rock.
    for x, y, a in ((40, 70, -70), (52, 64, -95), (64, 60, -80), (76, 62, -110), (88, 66, -60), (58, 74, -130), (82, 76, -45)):
        r = math.radians(a)
        x2, y2 = x + math.cos(r) * 30, y + math.sin(r) * 30
        d.line([P(x, y), P(x2, y2)], fill=INK, width=8 * S)
        d.line([P(x, y), P(x2, y2)], fill=rgb((238, 238, 232)), width=4 * S)
        for t in (0.35, 0.6, 0.85):
            bx, by = x + (x2 - x) * t, y + (y2 - y) * t
            d.line([P(bx, by), P(bx + math.cos(r + 0.9) * 8, by + math.sin(r + 0.9) * 8)], fill=rgb((250, 250, 246)), width=2 * S)
    d.line([P(26, 98), P(56, 92)], fill=HI, width=3 * S)
    done(im, 'saltpetre')


def clay():
    im, d = canvas()
    c = (132, 112, 92)
    poly(d, [(18, 92), (30, 58), (62, 44), (96, 52), (112, 84), (98, 106), (34, 108)], rgb(c))
    ell(d, 70 * S, 70 * S, 18 * S, 10 * S, shade(c, 0.7))
    d.line([P(58, 64), P(84, 66)], fill=shade(c, 0.5), width=3 * S)
    d.line([P(32, 70), P(48, 56)], fill=HI, width=4 * S)
    for x, y in ((40, 92), (88, 90), (64, 98)):
        ell(d, x * S, y * S, 3 * S, 2 * S, rgb((160, 140, 118)), width=0)
    done(im, 'clay')


def black_powder():
    im, d = canvas()
    paper = (214, 200, 168)
    poly(d, [(30, 30), (86, 22), (100, 70), (64, 92), (36, 74)], rgb(paper))
    d.line([P(36, 40), P(80, 32)], fill=shade(paper, 0.7), width=3 * S)
    poly(d, [(84, 22), (104, 12), (100, 30)], rgb(paper))   # the twist
    # Grains pouring out of the open end into a little heap.
    poly(d, [(50, 112), (96, 112), (88, 98), (62, 96)], rgb((36, 34, 38)))
    for x, y in ((66, 90), (72, 84), (60, 86), (78, 92), (70, 98), (84, 102)):
        ell(d, x * S, y * S, 2.5 * S, 2.5 * S, rgb((30, 28, 32)), width=0)
    d.line([P(38, 54), P(44, 70)], fill=HI, width=3 * S)
    done(im, 'black_powder')


def fire_pot():
    im, d = canvas()
    t = (178, 92, 52)
    poly(d, [(46, 46), (82, 46), (100, 70), (94, 100), (80, 112), (48, 112), (34, 100), (28, 70)], rgb(t))
    poly(d, [(54, 34), (74, 34), (78, 46), (50, 46)], rgb(shade(t, 0.8)))
    poly(d, [(52, 26), (76, 26), (74, 36), (54, 36)], rgb((206, 190, 150)))   # the cloth bung
    d.line([P(64, 26), P(70, 10)], fill=INK, width=7 * S)
    d.line([P(64, 26), P(70, 10)], fill=rgb((210, 196, 160)), width=3 * S)
    # The lit wick: a small flame.
    poly(d, [(70, 2), (78, 12), (72, 18), (64, 12)], rgb((255, 150, 40)), width=2 * S)
    poly(d, [(70, 7), (74, 12), (70, 15), (67, 12)], rgb((255, 230, 120)), width=0)
    d.line([P(36, 72), P(44, 98)], fill=HI, width=3 * S)
    d.line([P(34, 80), P(96, 80)], fill=shade(t, 0.65), width=3 * S)
    done(im, 'fire_pot')


def powder_charge():
    im, d = canvas()
    poly(d, [(16, 26), (52, 18), (60, 110), (24, 116)], rgb(OAK))                 # the plank, edge on
    d.line([P(20, 46), P(56, 40)], fill=rgb(IRON), width=5 * S)
    d.line([P(22, 92), P(58, 86)], fill=rgb(IRON), width=5 * S)
    # The petard: a squat bronze bell, mouth to the plank.
    poly(d, [(56, 30), (90, 38), (112, 56), (112, 82), (90, 100), (58, 106)], rgb(BRONZE))
    d.line([P(80, 36), P(84, 102)], fill=shade(BRONZE, 0.6), width=4 * S)
    ell(d, 104 * S, 52 * S, 4 * S, 4 * S, rgb(shade(BRONZE, 0.75)))
    # The slow match from its coil to the touch hole, its end glowing.
    ell(d, 36 * S, 66 * S, 9 * S, 9 * S, rgb(MATCH), width=3 * S)
    d.line([P(44, 62), P(70, 40), P(102, 48)], fill=INK, width=6 * S)
    d.line([P(44, 62), P(70, 40), P(102, 48)], fill=rgb(MATCH), width=3 * S)
    ell(d, 46 * S, 61 * S, 4 * S, 4 * S, rgb((255, 140, 40)), width=0)
    d.line([P(64, 40), P(64, 98)], fill=HI, width=3 * S)
    done(im, 'powder_charge')


def mining_charge():
    im, d = canvas()
    hide = (150, 104, 64)
    poly(d, [(18, 56), (88, 40), (96, 70), (26, 86)], rgb(hide))
    d.line([P(50, 48), P(58, 78)], fill=rgb(MATCH), width=4 * S)
    poly(d, [(86, 40), (104, 36), (112, 66), (94, 70)], rgb((150, 126, 98)))     # the clay plug
    d.line([P(108, 50), P(116, 30), P(104, 14)], fill=INK, width=6 * S)
    d.line([P(108, 50), P(116, 30), P(104, 14)], fill=rgb(MATCH), width=3 * S)
    d.line([P(26, 62), P(80, 50)], fill=HI, width=3 * S)
    # The rock it goes into: a drilled block below.
    poly(d, [(12, 98), (40, 88), (96, 92), (116, 110), (16, 118)], rgb((92, 88, 84)))
    done(im, 'mining_charge')


def siege_ladder():
    im, d = canvas()
    ash = (196, 160, 104)
    for x0, x1 in ((24, 58), (60, 94)):
        d.line([P(x0, 120), P(x1, 14)], fill=INK, width=11 * S)
        d.line([P(x0, 120), P(x1, 14)], fill=rgb(ash), width=6 * S)
    for k in range(1, 8):
        t = k / 8.0
        xa, ya = 24 + (58 - 24) * t, 120 - 106 * t
        xb, yb = 60 + (94 - 60) * t, 120 - 106 * t
        d.line([P(xa, ya), P(xb, yb)], fill=INK, width=7 * S)
        d.line([P(xa, ya), P(xb, yb)], fill=rgb(shade(ash, 0.85)), width=3 * S)
    for x in (58, 94):
        d.line([P(x, 14), P(x + 10, 6), P(x + 20, 16)], fill=INK, width=7 * S)
        d.line([P(x, 14), P(x + 10, 6), P(x + 20, 16)], fill=rgb(IRON), width=3 * S)
    done(im, 'siege_ladder')


def code_breaker():
    im, d = canvas()
    # A bronze frame holding a two-tined fork of pale violet crystal, ringing.
    poly(d, [(40, 112), (88, 112), (80, 74), (48, 74)], rgb(BRONZE))
    poly(d, [(46, 76), (56, 76), (58, 14), (48, 22)], rgb((190, 170, 230)))
    poly(d, [(72, 76), (82, 76), (80, 22), (70, 14)], rgb((190, 170, 230)))
    d.line([P(52, 70), P(53, 26)], fill=rgb((240, 230, 255)), width=2 * S)
    for r in (12, 20):
        d.arc([P(96 - r, 40 - r), P(96 + r, 40 + r)], -60, 60, fill=rgb((210, 196, 240)), width=3 * S)
        d.arc([P(30 - r, 40 - r), P(30 + r, 40 + r)], 120, 240, fill=rgb((210, 196, 240)), width=3 * S)
    d.line([P(46, 108), P(50, 80)], fill=HI, width=3 * S)
    done(im, 'code_breaker')


if __name__ == '__main__':
    for f in (saltpetre, clay, black_powder, fire_pot, powder_charge, mining_charge, siege_ladder, code_breaker):
        f()
