# The machines' sounds (cooks/build-machinery.md 2.10, 1.4): downloads the CC0 recordings below (each page is checked
# for its CC0 licence and that it links the file), then builds mono 3D loops that run seamlessly (an equal-power
# crossfade of the tail into the head) and short one-shots into Data/Audio/machine/. Docs/CREDITS.md lists them.
# Usage: python -I Tools/audio/build_machine_audio.py <work dir outside the repo> Data/Audio
# Needs numpy, scipy and soundfile (libsndfile 1.1+ for MP3). Never runs anything from the downloads.
import io, math, os, re, sys, urllib.request
import numpy as np, soundfile as sf
from scipy.signal import resample_poly, butter, sosfilt

WORK, OUT = sys.argv[1], os.path.join(sys.argv[2], "machine")
DL = os.path.join(WORK, "machine_dl")
SR = 44100
UA = {"User-Agent": "Mozilla/5.0 (game audio pass)"}
BSB, BF = "https://bigsoundbank.com/", "https://bigsoundbank.com/UPLOAD/ogg/"
FS = "https://freesound.org/"
SOURCES = {
    "mill_wheel": (BSB + "mill-wheel-bladed-s2768.html", BF + "2768.ogg"),
    "windmill_in": (FS + "people/pcwvdmark/sounds/345846/", "https://cdn.freesound.org/previews/345/345846_4622540-hq.mp3"),
    "wind_creak": (BSB + "wind-and-tree-squeaks-s1051.html", BF + "1051.ogg"),
    "steam_engine": (BSB + "steam-engine-foley-2-s3311.html", BF + "3311.ogg"),
    "steam_hiss": (BSB + "hiss-of-steam-train-1-s0227.html", BF + "0227.ogg"),
    "chain_hoist": (BSB + "manual-chain-hoist-downhill-s1134.html", BF + "1134.ogg"),
    "wood_gears": (FS + "people/hisoul/sounds/461166/", "https://cdn.freesound.org/previews/461/461166_1929421-hq.mp3"),
    "trip_hammer": (FS + "people/leinadsorihcak/sounds/449241/", "https://cdn.freesound.org/previews/449/449241_2362274-hq.mp3"),
    "millstone": (FS + "people/PLukx/sounds/430589/", "https://cdn.freesound.org/previews/430/430589_1944550-hq.mp3"),
    "sawmill": (BSB + "sawmill-3-s1538.html", BF + "1538.ogg"),
    "loom": (FS + "people/Na%C3%AFma/sounds/510266/", "https://cdn.freesound.org/previews/510/510266_8371354-hq.mp3"),
    "ratchet": (BSB + "large-ratchet-s0795.html", BF + "0795.ogg"),
    "pump": (BSB + "manual-water-pump-and-metal-bucket-s0542.html", BF + "0542.ogg"),
    "start": (BSB + "locomotive-a-vapeur-starting-s3021.html", BF + "3021.ogg"),
    "stop": (BSB + "steam-locomotive-stop-s3015.html", BF + "3015.ogg"),
}

def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
        return r.read()

def fetch():
    os.makedirs(DL, exist_ok=True)
    for key, (page, url) in SOURCES.items():
        path = os.path.join(DL, key + os.path.splitext(url)[1])
        if os.path.exists(path):
            continue
        text = get(page).decode("utf-8", "replace")
        cc0 = re.search(r"CC0|Creative Commons 0|publicdomain/zero", text) is not None
        if not cc0:
            raise SystemExit("not CC0 on its page: " + page)
        data = get(url)
        open(path, "wb").write(data)
        print("got", key, len(data), "bytes")

def load(key):
    for ext in (".ogg", ".mp3"):
        p = os.path.join(DL, key + ext)
        if os.path.exists(p):
            x, sr = sf.read(p, always_2d=True, dtype="float64")
            if sr != SR:
                g = math.gcd(SR, sr)
                x = resample_poly(x, SR // g, sr // g, axis=0)
            return x.mean(axis=1)
    raise FileNotFoundError(key)

def cut(x, a, b):
    return x[int(a * SR):int(b * SR)]

def highpass(x, hz):
    return sosfilt(butter(2, hz, "highpass", fs=SR, output="sos"), x)

def lowpass(x, hz):
    return sosfilt(butter(2, hz, "lowpass", fs=SR, output="sos"), x)

def rms_db(x):
    return 20 * math.log10(max(1e-9, float(np.sqrt(np.mean(x ** 2)))))

def level(x, target_db):
    return x * 10 ** ((target_db - rms_db(x)) / 20)

def loop(x, xfade):
    n = int(xfade * SR)
    t = np.linspace(0, 1, n)
    y = x[:-n].copy()
    y[:n] = x[:n] * np.sin(t * np.pi / 2) + x[-n:] * np.cos(t * np.pi / 2)
    return y

def fade(x, fin=0.004, fout=0.03):
    x = x.copy()
    i, o = int(fin * SR), int(fout * SR)
    if i: x[:i] *= np.linspace(0, 1, i)
    if o: x[-o:] *= np.linspace(1, 0, o)
    return x

def peak_safe(x, ceiling=0.9):
    pk = float(np.abs(x).max())
    return x * (ceiling / pk) if pk > ceiling else x

def write(name, x):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".ogg")
    data = np.clip(peak_safe(x), -1, 1).astype(np.float32)[:, None]
    with sf.SoundFile(path, "w", SR, 1, format="OGG", subtype="VORBIS", compression_level=0.45) as f:
        for i in range(0, len(data), 4096):
            f.write(data[i:i + 4096])
    print("wrote", path, round(len(x) / SR, 1), "s", os.path.getsize(path), "bytes")

def onset_hits(x, thresh_db=-20, gap=0.25, length=0.45):
    env = lowpass(np.abs(x), 30)
    th = env.max() * 10 ** (thresh_db / 20)
    hits, last = [], -1e9
    for i in np.where(env > th)[0]:
        if i - last > gap * SR:
            hits.append(i)
            last = i
    return [x[max(0, h - int(0.01 * SR)):h + int(length * SR)] for h in hits]

fetch()
# Loops (mono, for the 3D world; levels set so they sit under the ambience at 10 m).
write("waterwheel", loop(level(highpass(cut(load("mill_wheel"), 20, 52), 60), -24), 2.0))
wind = cut(load("windmill_in"), 3, 33)
creak = cut(load("wind_creak"), 10, 40)
write("windmill", loop(level(highpass(wind + 0.35 * level(creak, rms_db(wind)), 50), -24), 2.0))
steam = cut(load("steam_engine"), 1, 25)
hiss = load("steam_hiss")
roar = steam + 0.5 * level(np.resize(hiss, len(steam)), rms_db(steam))
write("turbine", loop(level(lowpass(roar, 6000), -23), 1.5))
chain = cut(load("chain_hoist"), 1, 18)
gears = cut(load("wood_gears"), 2, 19)
conv = chain[:min(len(chain), len(gears))] * 0.6 + gears[:min(len(chain), len(gears))]
write("conveyor", loop(level(highpass(conv, 120), -27), 1.0))
write("grinder", loop(level(highpass(cut(load("millstone"), 20, 50), 40), -24), 2.0))
write("sawmill", loop(level(highpass(cut(load("sawmill"), 30, 60), 80), -24), 1.5))
write("loom", loop(level(highpass(cut(load("loom"), 5, 35), 100), -25), 1.0))
write("pump", loop(level(highpass(cut(load("pump"), 2, 22), 80), -26), 1.0))
smelt = cut(load("steam_engine"), 4, 22)
write("smelter", loop(level(lowpass(resample_poly(smelt, 3, 2), 3000), -25), 1.5))   # the bellows: the chuff slowed
hum = cut(load("wood_gears"), 5, 20)
write("coil", loop(level(lowpass(hum, 1800), -30), 1.5))
# One-shots.
hits = onset_hits(load("trip_hammer"))
for i, h in enumerate(hits[:4]):
    write("drill_%d" % (i + 1), fade(level(h, -16)))
r = onset_hits(load("ratchet"), thresh_db=-18, gap=0.15, length=0.3)
for i, h in enumerate(r[:3]):
    write("sort_%d" % (i + 1), fade(level(h, -18)))
write("start", fade(level(highpass(cut(load("start"), 0, 4.5), 60), -20), 0.01, 0.6))
write("stop", fade(level(highpass(cut(load("stop"), 0, 4.0), 60), -20), 0.01, 0.6))
