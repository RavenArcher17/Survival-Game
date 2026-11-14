# Storm peaks sound pass: builds the biome's music, creature voices, hazard events and ambient beds into Data/Audio
# from the sources in sources_storm.py (Kevin MacLeod / incompetech music, Freesound CC0 / CC-BY recordings).
# Same rules as build_audio.py: short sounds by their loudest 400 ms (-16 LUFS), beds by integrated loudness (-23),
# music by integrated loudness (-18), true-ish peak under -1 dBFS, beds loop with an equal-power crossfade.
# Creature calls are cut from real animal recordings and fused in numpy (pitch by resampling, like asetrate;
# electric / material layers mixed under the call), as the cook's 2.12 voice recipes ask. ffmpeg decodes and encodes
# (Vorbis q3 for music, q4 for effects, q1 for the noise-like beds: the whole pass stays near 42 MB).
# Usage: python build_storm_audio.py <work> <Data/Audio> [music|voices|events|beds ...]
#   <work>/dl/music/<Title>.mp3 (incompetech), <work>/dl/fs/<freesound id>.mp3 (Freesound HQ previews)
# Needs numpy, scipy, soundfile and ffmpeg (FFMPEG env var or on PATH). Writes <work>/storm_manifest.json for credits.
import json, os, sys, math, subprocess, tempfile
import numpy as np, soundfile as sf
from scipy.signal import lfilter, resample_poly, butter, sosfilt
from scipy.ndimage import minimum_filter1d

WORK, OUT = sys.argv[1], sys.argv[2]
ONLY = set(sys.argv[3:])
FFMPEG = os.environ.get("FFMPEG", "ffmpeg")
SR = 44100
rng = np.random.default_rng(35)

def want(part):
    return not ONLY or part in ONLY

# ---------------------------------------------------------------- io
_cache = {}
def load(path):
    if path in _cache: return _cache[path].copy()
    raw = subprocess.run([FFMPEG, "-v", "error", "-i", path, "-f", "f32le", "-acodec", "pcm_f32le", "-ar", str(SR), "-ac", "2", "-"],
                         capture_output=True, check=True).stdout
    x = np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).astype(np.float64)
    _cache[path] = x
    return x.copy()

def fs(sid):
    return os.path.join(WORK, "dl", "fs", f"{sid}.mp3")

def music_src(title):
    return os.path.join(WORK, "dl", "music", title + ".mp3")

# ---------------------------------------------------------------- loudness (BS.1770, as build_audio.py)
def kweight(x, sr):
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = math.tan(math.pi * f0 / sr); Vh = 10 ** (G / 20); Vb = Vh ** 0.4996667741545416
    a0 = 1 + K / Q + K * K
    b = [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0]
    a = [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    y = lfilter(b, a, x, axis=0)
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = math.tan(math.pi * f0 / sr)
    a = [1, 2 * (K * K - 1) / (1 + K / Q + K * K), (1 - K / Q + K * K) / (1 + K / Q + K * K)]
    return lfilter([1, -2, 1], a, y, axis=0)

def blocks(x):
    y = kweight(x, SR)
    blk, hop = int(0.4 * SR), int(0.1 * SR)
    if len(y) < blk:
        return np.array([np.mean(np.sum(y ** 2, axis=1))])
    e = np.cumsum(np.concatenate([[0.0], np.sum(y ** 2, axis=1)]))
    idx = np.arange(0, len(y) - blk + 1, hop)
    return (e[idx + blk] - e[idx]) / blk

def lufs_integrated(x):
    ms = blocks(x); l = -0.691 + 10 * np.log10(ms + 1e-12)
    g = ms[l > -70]
    if len(g) == 0: return -70.0
    rel = -0.691 + 10 * np.log10(np.mean(g)) - 10
    g2 = ms[(l > -70) & (l > rel)]
    return float(-0.691 + 10 * np.log10(np.mean(g2) + 1e-12))

def lufs_max_momentary(x):
    return float(-0.691 + 10 * np.log10(blocks(x).max() + 1e-12))

def limit(x, ceiling_db=-1.0, release=0.05, look=0.002):
    thr = 10 ** (ceiling_db / 20)
    a = np.abs(x).max(axis=1)
    need = np.minimum(1.0, thr / np.maximum(a, 1e-9))
    if need.min() >= 1.0: return x
    n = max(1, int(look * SR))
    need = minimum_filter1d(need, size=2 * n + 1, mode="nearest")
    # hold for the release time, smooth, and never above what the peaks need (vectorised: music is long)
    h = max(1, int(release * SR))
    g = minimum_filter1d(need, size=h, mode="nearest")
    g = np.convolve(g, np.ones(h // 2 + 1) / (h // 2 + 1), mode="same")
    g = np.minimum(g, need)
    return x * g[:, None]

# ---------------------------------------------------------------- shaping
def mono(x): return x.mean(axis=1, keepdims=True) if x.shape[1] > 1 else x

def cut(x, a, b=None): return x[max(0, int(a * SR)): (int(b * SR) if b is not None else None)]

def trim(x, db=-45.0, pad=0.01):
    m = np.abs(x).max(axis=1)
    th = m.max() * 10 ** (db / 20)
    idx = np.where(m > th)[0]
    if len(idx) == 0: return x
    a = max(0, idx[0] - int(pad * SR)); b = min(len(x), idx[-1] + int(pad * SR * 4))
    return x[a:b]

def fade(x, fin=0.005, fout=0.03):
    x = x.copy(); n = len(x)
    i = min(int(fin * SR), n // 2); o = min(int(fout * SR), n // 2)
    if i > 0: x[:i] *= np.linspace(0, 1, i)[:, None]
    if o > 0: x[-o:] *= np.linspace(1, 0, o)[:, None]
    return x

def loop_xfade(x, secs):
    n = int(secs * SR)
    if n * 3 > len(x): n = len(x) // 3
    t = np.linspace(0, 1, n)[:, None]
    y = x[:-n].copy()
    y[:n] = x[:n] * np.sin(t * np.pi / 2) + x[-n:] * np.cos(t * np.pi / 2)
    return y

def stereo(x):
    if x.shape[1] >= 2: return x[:, :2]
    d = int(0.017 * SR)
    r = np.concatenate([x[-d:, 0], x[:-d, 0]])
    return np.stack([x[:, 0], r], axis=1)

def pitch(x, ratio):
    """Pitch and speed together (tape-style), like asetrate: ratio < 1 is lower and longer."""
    if abs(ratio - 1) < 1e-3: return x
    up, down = int(round(1000)), int(round(1000 * ratio))
    g = math.gcd(up, down)
    return resample_poly(x, up // g, down // g, axis=0)

def bandpass(x, lo=None, hi=None, order=4):
    if lo and hi: sos = butter(order, [lo, hi], btype="band", fs=SR, output="sos")
    elif lo: sos = butter(order, lo, btype="high", fs=SR, output="sos")
    else: sos = butter(order, hi, btype="low", fs=SR, output="sos")
    return sosfilt(sos, x, axis=0)

def env_follow(x, ms=20):
    a = np.abs(mono(x))[:, 0]
    n = max(1, int(ms / 1000 * SR))
    return np.convolve(a, np.ones(n) / n, mode="same")

def rms_db(x): return 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)

def match(x, n):
    if len(x) >= n: return x[:n]
    reps = int(np.ceil(n / max(1, len(x))))
    return np.concatenate([x] * reps)[:n]

def mix(base, layer, at=0.0, db=0.0):
    """Lays `layer` into `base` starting at `at` seconds, `db` relative (to its own level)."""
    s = int(at * SR)
    n = max(len(base), s + len(layer))
    out = np.zeros((n, base.shape[1]))
    out[:len(base)] += base
    L = layer if layer.shape[1] == base.shape[1] else np.repeat(mono(layer), base.shape[1], axis=1)
    out[s:s + len(L)] += L * 10 ** (db / 20)
    return out

def level_to(layer, ref, rel_db):
    """Scales `layer` so its RMS sits rel_db against `ref`'s RMS."""
    return layer * 10 ** ((rms_db(ref) + rel_db - rms_db(layer)) / 20)

def segments(x, thresh_db=-30, min_sil=0.15, min_len=0.12):
    m = np.abs(mono(x))[:, 0]; hop = int(SR * 0.01); n = len(m) // hop
    env = m[:n * hop].reshape(n, hop).max(axis=1)
    env = np.convolve(env, np.ones(3) / 3, mode="same")
    on = 20 * np.log10(env / (env.max() + 1e-9) + 1e-9) > thresh_db
    segs = []; i = 0
    while i < n:
        if on[i]:
            j = i; gap = 0
            while j < n and gap < int(min_sil / 0.01):
                gap = 0 if on[j] else gap + 1; j += 1
            end = j - gap
            if (end - i) * 0.01 >= min_len: segs.append((i * 0.01, end * 0.01))
            i = j
        else: i += 1
    return segs

def calls(sid, lo=0.15, hi=4.0, thresh=-30, min_sil=0.15, hp=None):
    """The separate calls in a recording, loudest first: [(peak_db, start, end, clip)]."""
    x = mono(load(fs(sid)))
    if hp: x = bandpass(x, lo=hp)
    out = []
    for a, b in segments(x, thresh, min_sil):
        if not (lo <= b - a <= hi): continue
        c = cut(x, a - 0.03, b + 0.12)
        out.append((20 * np.log10(np.abs(c).max() + 1e-9), a, b, c))
    out.sort(key=lambda r: -r[0])
    return out

def peaks(sid, n, length, band=None, pre=0.3, skip=0, gap=None, win=0.08):
    """For recordings with no clean silences: the n loudest moments (in `band`, Hz) at least `gap` s apart,
    each cut `length` s long starting `pre` of the way before its peak."""
    x = mono(load(fs(sid)))
    y = bandpass(x, *band) if band else x
    h = int(win * SR)
    e = np.sqrt(np.convolve(y[:, 0] ** 2, np.ones(h) / h, mode="same"))
    step = int(0.01 * SR)
    ed = e[::step]
    g = int((gap or length) / 0.01)
    order = np.argsort(-ed)
    taken = []
    for i in order:
        if len(taken) >= n + skip: break
        if all(abs(i - t) >= g for t in taken): taken.append(i)
    out = []
    for t in taken[skip:]:
        p = t * 0.01
        a = max(0.0, p - pre * length)
        out.append(fade(trim(cut(x, a, a + length), -40), 0.01, min(0.15, length * 0.2)))
    k = 0
    while len(out) < n:   # a short recording with fewer moments than asked: the same take, re-pitched a little
        out.append(pitch(out[k % len(out)], [0.94, 1.06, 0.9, 1.1][k % 4])); k += 1
    return out

def grab(sid, n, lo, hi, length=None, band=None, skip=0, **kw):
    """Separate calls between lo and hi s long if the recording has them, else its loudest moments."""
    cs = [c[3] for c in calls(sid, lo, hi, **kw)]
    if len(cs) >= n + skip: return [fade(c) for c in cs[skip:skip + n]]
    if len(cs) >= n: return [fade(c) for c in cs[-n:]]
    return peaks(sid, n, length or hi, band=band, skip=skip)

def longest(sid, lo=0.3, hi=8.0, **kw):
    cs = sorted(calls(sid, lo, hi, **kw), key=lambda r: -(r[2] - r[1]))
    return fade(cs[0][3]) if cs else peaks(sid, 1, hi)[0]

def ramp(x, a=0.0, b=1.0, power=1.0):
    return x * (np.linspace(a, b, len(x)) ** power)[:, None]

def tail_out(x, secs):
    x = x.copy(); n = min(len(x), int(secs * SR))
    x[-n:] *= np.linspace(1, 0, n)[:, None] ** 2
    return x

# ---------------------------------------------------------------- output
manifest = {}
mpath = os.path.join(WORK, "storm_manifest.json")
if os.path.exists(mpath): manifest = json.load(open(mpath))

def normalize_short(x, target, max_limit_db=10.0, ceiling_db=-1.0):
    for _ in range(3):
        L = lufs_max_momentary(x)
        g = 10 ** ((target - L) / 20)
        pk = np.abs(x).max() * g
        cap = 10 ** ((ceiling_db + max_limit_db) / 20)
        if pk > cap: g *= cap / pk
        x = limit(x * g, ceiling_db)
        if abs(lufs_max_momentary(x) - target) < 0.3: break
    return x

def normalize_long(x, target, ceiling_db=-1.0):
    L = lufs_integrated(x)
    x = x * 10 ** ((target - L) / 20)
    return limit(x, ceiling_db, release=0.15, look=0.004)

def write(name, x, kind, sources, q=None):
    path = os.path.join(OUT, name + ".ogg")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if kind == "sfx":
        x = fade(normalize_short(x, -16.0)); q = q or 4; ch = 1
        x = mono(x)
    elif kind == "bed":
        x = normalize_long(x, -23.0); q = q or 3; ch = 2
        x = stereo(x)
    else:
        x = fade(normalize_long(x, kind if isinstance(kind, float) else -18.0), 0.01, 0.05); q = q or 3; ch = 2
        x = stereo(x)
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, "a.wav")
        sf.write(wav, np.clip(x, -1, 1).astype(np.float32), SR, subtype="FLOAT")
        subprocess.run([FFMPEG, "-v", "error", "-y", "-i", wav, "-c:a", "libvorbis", "-q:a", str(q), "-ac", str(ch), path], check=True)
    manifest[name] = {"sources": sorted(set(str(s) for s in sources)), "seconds": round(len(x) / SR, 2),
                      "bytes": os.path.getsize(path)}
    print(f"{name:40} {len(x) / SR:7.2f}s {os.path.getsize(path) // 1024:6} KB", flush=True)

def save_manifest():
    json.dump(manifest, open(mpath, "w"), indent=1)

# ================================================================ music
MUSIC = [
    # file, incompetech title, max seconds (the director plays explore pieces <= 200 s), loop (crossfade) or fade out
    ("storm_explore_a", "Tempting Secrets", 200, False),
    ("storm_explore_b", "Master Disorder", 200, False),
    ("storm_explore_c", "Long Road Ahead", 200, False),
    ("storm_night_a", "Lost Frontier", 200, False),
    ("storm_night_b", "Decline", 200, False),
    ("storm_combat", "Hitman", 210, False),
    ("storm_mini", "Interloper", 200, False),
    ("storm_boss_a", "Corruption", 240, False),
    ("storm_boss_b", "Truth of the Legend", 240, False),
    ("storm_monastery", "Night Vigil", 210, False),
    ("storm_hollow_spire", "Thunder Dreams", 190, False),
    ("storm_camp", "Nu Flute", 200, False),
    ("storm_great_storm", "Arcane", 125, True),
    ("storm_pass_stormgut", "Phantasm", 200, False),
]
if want("music"):
    for name, title, mx, loop in MUSIC:
        x = load(music_src(title))
        x = trim(x, -60)
        if loop:
            # the war-drum layer loops under whatever plays: a 2-minute stretch from the driving middle, crossfaded
            x = loop_xfade(cut(x, 60, 60 + mx + 4), 4.0)
        elif len(x) > mx * SR:
            x = cut(x, 0, mx)
            n = int(8 * SR); x[-n:] *= np.linspace(1, 0, n)[:, None] ** 1.5
        write("music/" + name, x, -18.0, ["km:" + title])
    save_manifest()

# ================================================================ layers shared by the voices and events
def elec(sid, secs, which=0):
    """A stretch of a real electrical recording, secs long."""
    return match(peaks(sid, which + 1, secs, pre=0.5)[which], int(secs * SR))

def crackle(secs, which=0): return elec(871766, secs, which)        # sparking arc bed
def static(secs, which=0): return elec(875011, secs, which)         # static crackles and pops
def buzz(secs, which=0): return elec(256915, secs, which)           # arc welder buzz
def zap(which=0): return peaks(853286, which + 1, 1.2, pre=0.1)[which]
def discharge(which=0): return peaks(415962, which + 1, 1.5, pre=0.1)[which]
def thunder_tail(secs, which=0):
    return tail_out(bandpass(peaks(577315, which + 1, secs, pre=0.2)[which], hi=400), secs * 0.6)

def fused(call, layer, rel_db, at=0.0, shape=None):
    """A call with a material layer laid under or into it."""
    L = match(layer, len(call))
    if shape == "rise":   # the layer swells in toward the call's end ("breaks into crackling static at its peak")
        L = ramp(L, 0.0, 1.0, 2.0)
    elif shape == "follow":   # the layer rides the call's own loudness
        e = env_follow(call, 30)
        L = L * (e / (e.max() + 1e-9))[:, None]
    L = level_to(L, call, rel_db)
    return mix(call, L, at)

VOICE = {}
def voice(family, kind, clips, sources):
    for i, c in enumerate(clips):
        write(f"creature/{family}_{kind}_{i + 1}", c, "sfx", sources)
    VOICE.setdefault(family, {})[kind] = len(clips)

def head(x, secs): return fade(x[: int(secs * SR)], 0.005, 0.06)

# ================================================================ creature voices (cook 2.12 recipes)
if want("voices"):
    # Voltfang: grey wolf howls breaking into static at the peak, growls with an arc snap, whines.
    howls = grab(472402, 1, 2.0, 6.0) + grab(399186, 1, 2.0, 6.0) + peaks(143568, 1, 3.2, band=(300, 1500))
    voice("stormwolf", "aggro", [fused(h, crackle(len(h) / SR, i), -4, shape="rise") for i, h in enumerate(howls)],
          [472402, 399186, 143568, 871766])
    idle = peaks(158780, 2, 2.2, band=(250, 1500)) + peaks(500646, 1, 2.6, band=(300, 1200))
    voice("stormwolf", "idle", [fused(c, static(len(c) / SR, i), -20) for i, c in enumerate(idle)], [158780, 500646, 875011])
    snarls = grab(190595, 1, 0.6, 2.0) + peaks(472401, 2, 0.9, band=(120, 3000))
    voice("stormwolf", "attack", [mix(s, level_to(zap(i % 2), s, -6), 0.05) for i, s in enumerate(snarls)],
          [190595, 472401, 853286])
    whines = peaks(184808, 3, 0.6, band=(400, 3000))
    voice("stormwolf", "hurt", [fused(w, static(0.6, i), -14) for i, w in enumerate(whines)], [184808, 875011])
    d1 = tail_out(pitch(peaks(184808, 1, 1.6, band=(400, 3000), skip=3)[0], 0.88), 0.8)
    d2 = tail_out(pitch(howls[0], 0.8), 1.5)
    voice("stormwolf", "death", [fused(d1, crackle(len(d1) / SR, 1), -16), fused(d2, crackle(len(d2) / SR, 2), -12, shape="rise")],
          [184808, 472402, 871766])

    # Thunderhorn / Bellwether: rut grunts and roars (bighorn-like ram, bison, red stag), the horn-clash crack.
    roars = peaks(826421, 2, 2.6, band=(80, 2000)) + grab(186921, 1, 2.0, 3.5)
    voice("ram", "aggro", roars, [826421, 186921])
    voice("ram", "idle", peaks(521856, 3, 1.3, band=(80, 2000)), [521856])
    clash = grab(866683, 1, 0.1, 1.0)[0]
    grunts = grab(186921, 3, 0.15, 1.3)
    voice("ram", "attack", [mix(g, level_to(pitch(clash, [1.0, 0.85, 0.92][i]), g, 2), 0.12) for i, g in enumerate(grunts)],
          [186921, 866683])
    voice("ram", "hurt", grab(873888, 2, 0.3, 2.0) + peaks(519594, 1, 0.8, skip=2), [873888, 519594])
    voice("ram", "death", [tail_out(pitch(peaks(519594, 1, 1.6)[0], 0.85), 0.8), tail_out(pitch(roars[0], 0.88), 1.2)],
          [519594, 826421])

    # Stormkite / Galvara: red kite whistles, red-tailed and red-shouldered hawk cries, eagle squawks, condor.
    voice("raptor", "aggro", grab(868738, 1, 0.5, 3.0) + grab(868531, 2, 0.6, 2.0), [868738, 868531])
    voice("raptor", "idle", grab(865812, 2, 0.6, 1.6, skip=1) + grab(875513, 1, 0.4, 1.0), [865812, 875513])
    voice("raptor", "attack", grab(770032, 2, 0.25, 0.8) + grab(675962, 1, 0.3, 0.7), [770032, 675962])
    voice("raptor", "hurt", grab(875513, 2, 0.4, 1.0, skip=1) + grab(838963, 1, 0.3, 0.8, skip=2), [875513, 838963])
    voice("raptor", "death", [tail_out(pitch(grab(381200, 1, 0.5, 3.0)[0], 0.9), 0.8),
                              tail_out(pitch(grab(675962, 1, 0.8, 1.5)[0], 0.85), 0.5)], [381200, 675962])

    # Raiju: fisher-cat-like screams (red fox), yowls and snarls (cat), ferret; arc-welder buzz and a sizzling crack.
    screams = grab(634005, 3, 0.5, 1.0)
    voice("mustelid", "aggro", [mix(fused(s, buzz(len(s) / SR, i), -10, shape="follow"), level_to(discharge(i % 2), s, -4))
                                for i, s in enumerate(screams)], [634005, 256915, 415962])
    voice("mustelid", "idle", peaks(155115, 1, 1.2) + peaks(679954, 1, 1.8) + grab(750495, 1, 0.3, 0.8), [155115, 679954, 750495])
    yowls = grab(750495, 3, 0.9, 1.8)
    voice("mustelid", "attack", [fused(y, buzz(len(y) / SR, i + 1), -8, shape="follow") for i, y in enumerate(yowls)],
          [750495, 256915])
    voice("mustelid", "hurt", grab(634005, 3, 0.3, 0.7, skip=3), [634005])
    voice("mustelid", "death", [tail_out(pitch(peaks(554308, 1, 2.0)[0], 0.9), 1.0),
                                tail_out(pitch(grab(750495, 1, 1.0, 2.0, skip=3)[0], 0.85), 0.8)], [554308, 750495])

    # Cragstrider: the alarm sneeze-whistle (goat snort + marmot whistle, fused), bleats, hoof-on-slate knock.
    sneeze = grab(587167, 1, 0.08, 1.0)[0]
    whistles = grab(822939, 3, 0.5, 1.5)
    voice("goat", "aggro", [mix(level_to(sneeze, w, 0), w, 0.08) for w in whistles], [587167, 822939])
    voice("goat", "idle", grab(391320, 3, 0.4, 1.8), [391320])
    bleats = peaks(722658, 3, 0.7)
    voice("goat", "attack", [mix(level_to(pitch(sneeze, 0.95), b, 0), level_to(pitch(clash, 1.3), b, -2), 0.05) for b in bleats[:2]]
          + [mix(bleats[2], level_to(pitch(clash, 1.2), bleats[2], -3), 0.1)], [587167, 866683, 722658])
    voice("goat", "hurt", peaks(352600, 3, 0.6), [352600])
    voice("goat", "death", [tail_out(pitch(grab(391320, 1, 1.5, 3.0)[0], 0.88), 1.0), tail_out(pitch(bleats[0], 0.8), 0.5)],
          [391320, 722658])

    # Galeback: yak grunts, bison huffs, camel groan; the grunt chorus as the herd rings up.
    yg = peaks(834331, 6, 1.1, band=(60, 1500))
    camel = grab(437937, 1, 1.0, 2.0)[0]
    voice("yak", "aggro", [mix(camel, level_to(yg[0], camel, -3), 0.3),
                           mix(mix(yg[1], level_to(pitch(yg[2], 0.92), yg[1], -2), 0.25), level_to(pitch(yg[3], 1.06), yg[1], -4), 0.5)],
          [437937, 834331])
    voice("yak", "idle", yg[3:6], [834331])
    voice("yak", "attack", grab(873888, 1, 1.0, 2.5) + peaks(867549, 1, 1.5), [873888, 867549])
    voice("yak", "hurt", [pitch(head(yg[0], 0.6), 1.12), pitch(head(yg[4], 0.6), 1.08)], [834331])
    voice("yak", "death", [tail_out(pitch(camel, 0.85), 1.0), tail_out(pitch(peaks(519594, 1, 1.8, skip=1)[0], 0.8), 1.0)],
          [437937, 519594])

    # Coilburrow / Coilback: churrs (guinea pig purr over a field-cricket trill), hedgehog huffs, rat chitter; soil, spark ticks.
    purrs = grab(583077, 3, 0.9, 1.8)
    cricket = peaks(571206, 1, 2.0, band=(2500, 9000))[0]
    sparkt = peaks(643861, 1, 0.4, pre=0.05)[0]
    def ticks(n_len, k):
        base = np.zeros((int(n_len * SR), 1))
        for j in range(k):
            base = mix(base, sparkt * 0.6, (j + 0.5) * n_len / (k + 1))
        return base
    voice("mole", "idle", [mix(fused(p, cricket, -14, shape="follow"), level_to(ticks(len(p) / SR, 3 + i), p, -10))
                           for i, p in enumerate(purrs)], [583077, 571206, 643861])
    huffs = peaks(528183, 3, 0.9)
    voice("mole", "aggro", [fused(h, cricket, -10, shape="follow") for h in huffs], [528183, 571206])
    crumble = grab(348955, 1, 0.3, 2.5)[0]
    voice("mole", "attack", [mix(head(h, 0.6), level_to(crumble, h, -4), 0.05) for h in huffs[:2]], [528183, 348955])
    voice("mole", "hurt", peaks(841571, 3, 0.6), [841571])
    voice("mole", "death", [tail_out(mix(pitch(purrs[0], 0.8), level_to(crumble, purrs[0], -6), 0.4), 1.0),
                            tail_out(pitch(peaks(841571, 1, 1.2, skip=3)[0], 0.75), 0.6)], [583077, 348955, 841571])

    # Rodcrest: monitor-like hisses (snake), gecko chirps, tokay calls.
    hisses = grab(678971, 3, 0.8, 1.6)
    tokay = grab(686626, 1, 1.0, 3.0)[0]
    voice("lizard", "aggro", [mix(tokay, level_to(hisses[0], tokay, -2), 0.6), hisses[1], grab(553374, 1, 1.0, 3.0)[0]],
          [686626, 678971, 553374])
    voice("lizard", "idle", grab(351301, 3, 0.15, 1.0), [351301])
    voice("lizard", "attack", [head(h, 0.55) for h in hisses], [678971])
    voice("lizard", "hurt", grab(351301, 2, 0.15, 1.0, skip=3) + peaks(686626, 1, 0.5, skip=1), [351301, 686626])
    voice("lizard", "death", [tail_out(pitch(tokay, 0.8), 1.0), tail_out(pitch(hisses[2], 0.85), 0.8)], [686626, 678971])

    # Puffram: sheep and lamb bleats; wool static.
    voice("sheep", "idle", grab(471162, 3, 0.6, 1.3), [471162])
    big = grab(362283, 2, 1.5, 4.5) + grab(383144, 1, 1.8, 3.0)
    voice("sheep", "aggro", [fused(b, static(len(b) / SR, i), -18) for i, b in enumerate(big)], [362283, 383144, 875011])
    voice("sheep", "attack", grab(362283, 2, 0.5, 1.2, skip=2) + grab(471162, 1, 0.5, 1.2, skip=3), [362283, 471162])
    lamb = grab(266689, 1, 1.0, 3.5)[0]
    voice("sheep", "hurt", [head(lamb, 0.9), head(grab(383144, 1, 0.8, 2.5, skip=3)[0], 0.8)], [266689, 383144])
    voice("sheep", "death", [tail_out(pitch(big[2], 0.85), 1.2), tail_out(pitch(lamb, 0.8), 1.0)], [383144, 266689])

    # Kharvox: an eagle / condor shriek an octave down, a lion roar sub layer, thunder rolling off the big calls.
    condor = grab(838963, 4, 0.4, 1.2)
    lions = grab(69572, 3, 1.0, 2.2)
    def roc(shriek, roar, p, roar_db, thunder=None):
        s = pitch(shriek, p)
        out = mix(s, level_to(pitch(roar, 0.8), s, roar_db), 0.05)
        if thunder is not None: out = mix(out, level_to(thunder, out, -4), len(s) / SR * 0.5)
        return out
    eaglecry = grab(381200, 1, 0.5, 3.0)[0]
    voice("roc", "aggro", [roc(condor[0], lions[0], 0.5, -3, thunder_tail(4.0, 0)), roc(eaglecry, lions[1], 0.55, -4, thunder_tail(4.0, 1)),
                           roc(condor[1], lions[2], 0.52, -2)], [838963, 381200, 69572, 577315])
    eag = grab(675962, 3, 0.8, 1.5)
    growl = grab(611721, 1, 0.8, 1.5)[0]
    voice("roc", "idle", [roc(e, growl, 0.6, -8) for e in eag[:2]], [675962, 611721])
    lr = grab(559600, 2, 1.0, 1.8)
    voice("roc", "attack", [roc(condor[2], lr[0], 0.55, -2), roc(condor[3], lr[1], 0.55, -2), roc(condor[0], lions[0], 0.6, 0)],
          [838963, 559600, 69572])
    voice("roc", "hurt", [roc(c, head(lions[1], 0.8), 0.62, -5) for c in grab(770032, 2, 0.25, 0.8)], [770032, 69572])
    bigroar = grab(611721, 1, 3.0, 5.0)[0]
    voice("roc", "death", [tail_out(roc(eaglecry, bigroar, 0.5, -2, thunder_tail(5.0, 2)), 2.0),
                           tail_out(roc(eag[2], lions[2], 0.5, -3, thunder_tail(5.0, 0)), 2.0)], [381200, 611721, 675962, 69572, 577315])
    json.dump(VOICE, open(os.path.join(WORK, "storm_voices.json"), "w"))
    save_manifest()

# ================================================================ events (hazards and the boss)
EVENTS = {}
def event(name, clips, sources):
    files = []
    for i, c in enumerate(clips):
        f = f"world/storm_{name}_{i + 1}"
        write(f, c, "sfx", sources)
        files.append(f)
    EVENTS[name] = files

def cracked(b, p, detune=1.035):
    """A bell made to sound cracked: the strike and a slightly sharp copy beating against it."""
    x = pitch(b, p)
    return tail_out(mix(x, level_to(pitch(b, p * detune), x, -5), 0.004), 0.3)

if want("events"):
    asc = peaks(145506, 2, 3.0, pre=0.5)
    hum = peaks(776446, 2, 3.0, pre=0.5)
    event("pretell", [ramp(mix(a, level_to(h, a, -3)), 0.05, 1.0, 2.2) for a, h in zip(asc, hum)], [145506, 776446])
    event("strike", [peaks(sid, 1, 4.5, pre=0.04)[0] for sid in (434359, 437243, 840628)], [434359, 437243, 840628])
    event("thunder", [tail_out(peaks(577315, 1, 10.0, pre=0.25)[0], 3), tail_out(peaks(572443, 1, 10.0, pre=0.25)[0], 3),
                      tail_out(peaks(577315, 2, 9.0, pre=0.25, gap=12)[1], 3)], [577315, 572443])
    event("ground_current", [ramp(crackle(2.0, 0), 1, 0.4), ramp(buzz(2.0, 0), 1, 0.4)], [871766, 256915])
    gusts = [peaks(770282, 1, 1.4, pre=0.7)[0], peaks(838744, 1, 1.4, pre=0.7)[0], peaks(352421, 1, 1.4, pre=0.7)[0]]
    event("gust", [ramp(g, 0.2, 1.0, 1.5) for g in gusts], [770282, 838744, 352421])
    event("plank_creak", grab(506665, 1, 0.3, 1.5) + grab(618085, 1, 0.3, 2.0) + peaks(130364, 1, 1.4), [506665, 618085, 130364])
    event("plank_snap", peaks(400645, 2, 1.2, pre=0.1) + peaks(873248, 1, 1.2, pre=0.1), [400645, 873248])
    event("rockfall_trickle", grab(521956, 1, 0.5, 4.0) + peaks(179341, 1, 2.5, pre=0.2), [521956, 179341])
    event("rockfall_impact", [peaks(500638, 1, 3.5, pre=0.15)[0], peaks(381645, 1, 3.0, pre=0.1)[0], peaks(389303, 1, 3.0, pre=0.1)[0]],
          [500638, 381645, 389303])
    h1, h2 = peaks(453445, 1, 3.0, pre=0.5)[0], peaks(721812, 1, 3.0, pre=0.5)[0]
    event("ball_hum", [mix(h1, level_to(crackle(3.0, 1), h1, -10)), mix(h2, level_to(static(3.0, 1), h2, -10))],
          [453445, 721812, 871766, 875011])
    event("ball_pop", [mix(zap(0), level_to(discharge(0), zap(0), 0)), discharge(1)], [853286, 415962])
    event("rod_hum", [peaks(160469, 1, 4.0, pre=0.5)[0], peaks(157280, 1, 4.0, pre=0.5)[0]], [160469, 157280])
    cow = grab(350711, 1, 0.3, 2.5)[0]
    event("bell_clank", [cracked(cow, 0.55), cracked(cow, 0.6, 1.05), cracked(cow, 0.52, 1.028)], [350711])
    tolls = peaks(454855, 2, 5.0, pre=0.05, gap=4.0)
    big_bell = peaks(674043, 1, 5.0, pre=0.05)[0]
    event("bell_toll", [cracked(t, 0.85, 1.03) for t in tolls] + [cracked(big_bell, 0.8, 1.04)], [454855, 674043])
    event("arc", [zap(0), peaks(415962, 3, 0.8, pre=0.1)[2], peaks(362975, 1, 0.9, pre=0.1)[0]], [853286, 415962, 362975])
    event("charge_whine", [ramp(pitch(a, 1.15), 0.1, 1.0, 1.5) for a in peaks(145506, 2, 2.5, pre=0.5, skip=2)], [145506])
    wings = peaks(596541, 2, 1.2, pre=0.3) + peaks(711122, 1, 1.2, pre=0.3)
    event("wingbeat", [mix(pitch(w, 0.65), level_to(thunder_tail(3.0, i % 2), pitch(w, 0.65), -6), 0.15) for i, w in enumerate(wings)],
          [596541, 711122, 577315])
    event("clap", [peaks(sid, 1, 4.0, pre=0.03)[0] for sid in (616992, 200990, 437243)], [616992, 200990, 437243])
    event("cloud_static", [peaks(503139, 1, 3.0)[0], static(3.0, 2)], [503139, 875011])
    event("ring_buzz", [ramp(bandpass(c, hi=900), 0.05, 1.0, 1.8) for c in peaks(776446, 2, 3.0, skip=2)], [776446])
    event("burrow", peaks(696531, 1, 1.5) + peaks(384362, 1, 1.5) + grab(348955, 1, 0.3, 2.5), [696531, 384362, 348955])
    event("spark", [peaks(643861, 1, 0.4, pre=0.05)[0]] + peaks(232841, 2, 0.35, pre=0.1), [643861, 232841])
    z = zap(0)
    event("shrine", [mix(z, level_to(cracked(head(tolls[0], 3.0), 1.2, 1.0), z, -3), 0.08),
                     mix(discharge(0), level_to(cracked(head(big_bell, 3.0), 1.1, 1.0), z, -3), 0.1)],
          [853286, 415962, 454855, 674043])
    json.dump(EVENTS, open(os.path.join(WORK, "storm_events.json"), "w"))
    save_manifest()

# ================================================================ ambient beds (~60 s loops)
def bed_src(sid, secs=64, start=None):
    x = load(fs(sid))
    if start is None: start = max(0.0, (len(x) / SR - secs) / 2)
    return match(cut(x, start, start + secs), int(secs * SR))

def sprinkle(base, clips, every, db, seed):
    """Scatters one-shot clips (calls, gusts, creaks) through a bed at random times and levels."""
    r = np.random.default_rng(seed); t = r.uniform(2, every)
    out = base
    while t < len(base) / SR - 6:
        c = stereo(clips[r.integers(len(clips))])
        out = mix(out, level_to(c, base, db + r.uniform(-3, 2)), t)
        t += r.uniform(every * 0.6, every * 1.4)
    return out[: len(base)]

BEDS = {}
def bed(name, x, sources):
    write(f"amb/storm_{name}", loop_xfade(x, 4.0), "bed", sources, q=1)
    BEDS[name] = f"amb/storm_{name}"

if want("beds"):
    wind = bed_src(454092)
    bed("peaks", wind, [454092])
    birds = bed_src(288899)
    eagles = grab(675962, 4, 0.8, 1.5) + grab(865812, 2, 0.6, 1.6)
    bed("peaks_day", sprinkle(mix(birds, level_to(wind, birds, -6)), [bandpass(e, hi=5000) for e in eagles], 14, -12, 1),
        [288899, 454092, 675962, 865812])
    cold = bed_src(156414)
    bed("peaks_night", mix(bandpass(cold, hi=3000), level_to(bandpass(wind, hi=1200), cold, -8)), [156414, 454092])
    grass = bed_src(672330); sea = bed_src(698546)
    bed("passes", sprinkle(mix(grass, level_to(bandpass(sea, hi=1500), grass, -6)), peaks(352421, 2, 2.5) + peaks(770282, 1, 2.5), 12, -4, 2),
        [672330, 698546, 352421, 770282])
    hollow = bed_src(157280); fall = bed_src(650699)
    bed("cliffs", mix(bandpass(hollow, 150, 4000), level_to(fall, hollow, -7)), [157280, 650699])
    river = bed_src(555736) if len(load(fs(555736))) > 40 * SR else bed_src(588180)
    gorge = bandpass(river, hi=900)
    gorge = mix(gorge, gorge * 0.35, 0.21)[: len(gorge)]       # one slap back off the gorge wall
    bed("bridges", sprinkle(mix(gorge, level_to(bandpass(wind, hi=2500), gorge, -6)), peaks(145721, 4, 3.0), 9, -6, 3),
        [555736, 588180, 145721, 454092])
    song = match(stereo(peaks(838744, 1, 12.0)[0]), len(wind))
    bed("crags", sprinkle(mix(wind, level_to(song, wind, -4)), [static(1.5, k) for k in range(3)], 7, -12, 4), [454092, 838744, 875011])
    chimes = bed_src(437337); creak = bed_src(130364)
    bed("shrines", mix(mix(chimes, level_to(bandpass(wind, hi=2000), chimes, -4)), level_to(creak, chimes, -10)),
        [437337, 130364, 454092])
    harp = bed_src(826812); ant = bed_src(160469)
    bed("rodfield", mix(harp, level_to(ant, harp, -5)), [826812, 160469])
    drips = bed_src(870748); mine = bed_src(627789)
    hum = match(stereo(bandpass(peaks(453445, 1, 20.0)[0], hi=300)), len(drips))
    bed("hollow", mix(mix(drips, level_to(mine, drips, -4)), level_to(hum, drips, -10)), [870748, 627789, 453445])
    bed("thunderstorm", bed_src(435222, 75), [435222])
    json.dump(BEDS, open(os.path.join(WORK, "storm_beds.json"), "w"))
    save_manifest()
