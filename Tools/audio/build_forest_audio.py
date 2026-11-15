# Ancient forest (Nature) sound pass: builds the biome's music, creature voices, hazard / place / boss events, the Nature
# path's sounds and ambient beds into Data/Audio from the sources in sources_forest.py (Kevin MacLeod / incompetech music,
# Freesound CC0 / CC-BY recordings). The Storm and Swamp passes' rules (build_storm_audio.py, build_swamp_audio.py): short
# sounds by their loudest 400 ms (-16 LUFS), beds by integrated loudness (-23), music by integrated loudness per the cook's
# 2.16 mix (-18 explore and combat, -23 the calm cues, -15 the boss), true-ish peak under -1 dBFS, beds and loops with an
# equal-power crossfade. Creature calls are cut from real animal recordings and fused in numpy (pitch by resampling, like
# asetrate; each family's material layer mixed under the call), as cooks/biome-forest.md 2.15 asks: 3+ real calls per
# family, tells rising in pitch. ffmpeg decodes and encodes (Vorbis q3 music, q4 effects, q1 beds).
# Usage: python build_forest_audio.py <work> <Data/Audio> [music|voices|events|nature|beds ...]
#   <work>/dl/music/<Title>.mp3 (incompetech), <work>/dl/fs/<freesound id>.mp3 (Freesound HQ previews)
# Needs numpy, scipy, soundfile and ffmpeg (FFMPEG env var or on PATH). Writes <work>/forest_manifest.json for credits.
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
mpath = os.path.join(WORK, "forest_manifest.json")
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


# ================================================================ layers shared by the music, voices and events
def glide(x, s0=0.0, s1=3.0):
    """Pitch (and speed) gliding from s0 to s1 semitones over the clip: the cook's tells rise in pitch."""
    n = len(x)
    r = 2 ** (np.linspace(s0, s1, n) / 12)
    pos = np.cumsum(r) - r[0]
    pos = pos[pos < n - 1]
    i = pos.astype(int); f = (pos - i)[:, None]
    return x[i] * (1 - f) + x[i + 1] * f

def rev(x): return x[::-1].copy()

def head(x, secs): return fade(x[: int(secs * SR)], 0.005, 0.06)

def tremble(x, hz=18.0, depth=0.6):
    """A surface shivering: the layer's level trembles at hz (a bee ball's thrum)."""
    t = np.arange(len(x)) / SR
    return x * (1 - depth * 0.5 * (1 + np.sin(2 * np.pi * hz * t)))[:, None]

def echoes(x, taps=((0.23, -7), (0.47, -12), (0.74, -17), (1.05, -23)), lp=1800):
    """Taps off walls and trunks: each later and duller."""
    out = x
    for d, db in taps:
        out = mix(out, bandpass(x, hi=lp) * 10 ** (db / 20), d)
    return out

def fused(call, layer, rel_db, at=0.0, shape=None):
    """A call with a material layer laid under or into it (build_storm_audio.py's rule)."""
    L = match(layer, len(call))
    if shape == "rise":
        L = ramp(L, 0.0, 1.0, 2.0)
    elif shape == "follow":
        e = env_follow(call, 30)
        L = L * (e / (e.max() + 1e-9))[:, None]
    L = level_to(L, call, rel_db)
    return mix(call, L, at)

def chorus(clips, gaps, pitches, dbs=None):
    """Several calls in a ragged run (a murder of crows, a ring of toads): each starts gaps[i] s after the first."""
    out = pitch(clips[0], pitches[0])
    for i in range(1, len(clips)):
        c = pitch(clips[i], pitches[i % len(pitches)])
        out = mix(out, level_to(c, out, (dbs[i] if dbs else -2.0)), gaps[i])
    return out

def seq(clips, gap):
    """Clips one after another, gap s apart (start to start)."""
    out = clips[0]
    for i, c in enumerate(clips[1:]):
        out = mix(out, c, gap * (i + 1))
    return out

def part(sid, secs, which=0, band=None):
    """A stretch of a texture recording (water, mud, slime), secs long."""
    return match(peaks(sid, which + 1, secs, pre=0.5, band=band)[which], int(secs * SR))

VOICE = {}
def voice(family, kind, clips, sources):
    for i, c in enumerate(clips):
        write(f"creature/{family}_{kind}_{i + 1}", c, "sfx", sources)
    VOICE.setdefault(family, {})[kind] = len(clips)


# ---------------------------------------------------------------- bed helpers (also used by the Bloom layer)
def tile(x, n, xf=2.5, seed=0):
    """A short recording grown to n samples by laying stretches of it end to end with equal-power crossfades, each
    from a different point (no hard seams, no audible restart)."""
    if len(x) >= n: return x[:n]
    r = np.random.default_rng(seed); k = int(xf * SR)
    seg = max(k * 3, len(x) - int(0.5 * SR))
    out = x[:seg].copy()
    t = np.linspace(0, 1, k)[:, None]
    while len(out) < n:
        a = int(r.integers(0, max(1, len(x) - seg + 1))) if len(x) > seg else 0
        nxt = x[a:a + seg]
        if len(nxt) <= k: nxt = x[:seg]
        out = np.concatenate([out[:-k], out[-k:] * np.cos(t * np.pi / 2) + nxt[:k] * np.sin(t * np.pi / 2), nxt[k:]])
    return out[:n]

def bed_src(sid, secs=64, start=None):
    x = load(fs(sid))
    if len(x) < secs * SR: return tile(x, int(secs * SR), seed=sid % 1000)
    if start is None: start = max(0.0, (len(x) / SR - secs) / 2)
    return cut(x, start, start + secs)[: int(secs * SR)]

def sprinkle(base, clips, every, db, seed):
    """Scatters one-shot clips (calls, creaks, bubbles) through a bed at random times and levels."""
    r = np.random.default_rng(seed); t = r.uniform(2, every)
    out = base
    while t < len(base) / SR - 6:
        c = stereo(clips[r.integers(len(clips))])
        out = mix(out, level_to(c, base, db + r.uniform(-3, 2)), t)
        t += r.uniform(every * 0.6, every * 1.4)
    return out[: len(base)]


def write_loop(name, x, sources, target=-20.0, q=4):
    """A seamless mono loop (StartLoop plays Files[0] round and round): crossfaded end into start, no fades."""
    path = os.path.join(OUT, name + ".ogg")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    x = mono(normalize_long(loop_xfade(x, min(1.0, len(x) / SR / 4)), target))
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, "a.wav")
        sf.write(wav, np.clip(x, -1, 1).astype(np.float32), SR, subtype="FLOAT")
        subprocess.run([FFMPEG, "-v", "error", "-y", "-i", wav, "-c:a", "libvorbis", "-q:a", str(q), "-ac", "1", path], check=True)
    manifest[name] = {"sources": sorted(set(str(s) for s in sources)), "seconds": round(len(x) / SR, 2), "bytes": os.path.getsize(path)}
    print(f"{name:40} {len(x) / SR:7.2f}s {os.path.getsize(path) // 1024:6} KB (loop)", flush=True)

# ================================================================ music
MUSIC = [
    # file, incompetech title, start s, max s (the director lets an explore piece run <= 200 s), loop (crossfade), loudness.
    # Cook 2.16 mix: -18 LUFS explore / combat / minis, calm cues -23 (night, the Hollow Root, camp, village), the boss -15.
    ("forest_explore_a", "Morning", 0, 160, False, -18.0),
    ("forest_explore_a2", "Ascending the Vale", 0, 180, False, -18.0),
    ("forest_explore_b", "Skye Cuillin", 0, 180, False, -18.0),
    ("forest_explore_b2", "Decline", 0, 180, False, -18.0),
    ("forest_explore_c", "Night Vigil", 0, 180, False, -18.0),
    ("forest_explore_c2", "Frozen Star", 0, 180, False, -18.0),
    ("forest_night_a", "Evening Fall (Harp)", 0, 150, False, -23.0),
    ("forest_night_b", "Enchanted Journey", 0, 170, False, -23.0),
    ("forest_combat", "Celtic Impulse", 0, 150, False, -18.0),
    ("forest_combat_b", "Achaidh Cheide", 0, 134, False, -18.0),
    ("forest_mini", "Lord of the Land", 0, 150, False, -18.0),
    ("forest_old_gnarl", "Dragon and Toast", 0, 150, False, -18.0),
    ("forest_bramblequeen", "Darkest Child", 0, 150, False, -18.0),
    ("forest_grey_matron", "The Chamber", 0, 114, False, -18.0),
    # Sylvaroth: one piece per phase (P1 the low choir, P2 the drums and choir, P3 everything in), each looping.
    ("forest_sylvaroth_p1", "Ancient Rite", 0, 110, True, -15.0),
    ("forest_sylvaroth_p2", "Final Count", 30, 120, True, -15.0),
    ("forest_sylvaroth_p3", "Minima", 40, 120, True, -15.0),
    ("forest_camp", "Canon in D for Two Renaissance Harps", 0, 120, False, -23.0),
    ("forest_village", "Village Consort", 0, 120, False, -23.0),
]
if want("music"):
    for name, title, start, mx, loop, lufs in MUSIC:
        x = trim(load(music_src(title)), -60)
        if loop:
            x = loop_xfade(cut(x, start, start + mx + 4), 4.0)
        else:
            x = cut(x, start, start + mx) if len(x) > (start + mx) * SR or start else x
            if len(x) >= mx * SR - SR:
                n = int(8 * SR); x[-n:] *= np.linspace(1, 0, n)[:, None] ** 1.5
        write("music/" + name, x, lufs, ["km:" + title], q=2)   # q2 keeps the forest pass under ~50 MB
    # The Hollow Root (cook 2.16): Arcadia's choirs and celesta with a slow low pulse under it, like the Thrumming Root's
    # heartbeat; looping (the cue plays while you are down there).
    arc = trim(load(music_src("Arcadia")), -60)
    beats = [bandpass(pitch(b, 0.7), hi=180) for b in grab(249716, 4, 0.3, 1.5)]
    pulse = np.zeros_like(arc); t = 0.5; k = 0
    while t < len(arc) / SR - 2:
        b = stereo(beats[k % 4]); s = int(t * SR); n = min(len(b), len(pulse) - s)
        pulse[s:s + n] += b[:n]; t += 1.25; k += 1
    hol = mix(arc, level_to(pulse, arc, -9))[: len(arc)]
    write("music/forest_hollow_root", loop_xfade(hol, 4.0), -23.0, ["km:Arcadia", 249716], q=2)
    # The Wild Hunt layer (the Bloom's night): Dentaneosuchus Hunt's log drums with the hunting horns calling over them
    # and the hooves under, a 60 s loop laid over whatever plays.
    hunt = cut(trim(load(music_src("Dentaneosuchus Hunt")), -60), 20, 84)
    horns = [stereo(h) for h in grab(69206, 2, 0.8, 6.0) + grab(148645, 1, 0.5, 4.0) + grab(361684, 1, 1.0, 8.0)]
    hooves = stereo(bandpass(match(load(fs(479742)), len(hunt)), 60, 3000))
    w = mix(hunt, level_to(hooves, hunt, -10))[: len(hunt)]
    for i, at in enumerate((3.0, 18.0, 33.0, 48.0)):
        w = mix(w, level_to(horns[i % len(horns)], hunt, -4), at)[: len(hunt)]
    write("music/forest_wild_hunt", loop_xfade(w, 3.0), -20.0, ["km:Dentaneosuchus Hunt", 69206, 148645, 361684, 479742], q=2)
    # The Bloom's layer: fuller birdsong (a dawn chorus with blackbird and song thrush close) and a high chime shimmer
    # like a celesta, over the normal cue (no melody of its own, so it never fights the piece under it).
    dawn = load(fs(611453))[: int(64 * SR)]
    bb = grab(725332, 4, 0.8, 4.0) + grab(785669, 3, 0.8, 4.0)
    shim = [bandpass(pitch(c, 1.5), lo=1500) for c in grab(437337, 3, 0.5, 4.0)]
    lay = bandpass(dawn, lo=500)
    lay = sprinkle(lay, bb, 5, -2, 31)
    lay = sprinkle(lay, shim, 7, -6, 32)
    write("music/forest_bloom", loop_xfade(lay, 4.0), -22.0, [611453, 725332, 785669, 437337], q=2)
    save_manifest()

# ================================================================ creature voices (cook 2.15 recipes)
# Kinds: idle, aggro, attack, hurt, death (WRSound::Voice), tell (the wind-up, rising in pitch, sitting ~3 dB over the bed in
# the table) and signature (the cook's signature moment). Every family: 3+ real animal calls and a material layer.
if want("voices"):
    leaves = grab(422963, 3, 0.3, 4.0)                    # a leaf shower
    moss = part(853612, 3.0)                              # moss, fern and foliage handled
    claw = grab(830466, 4, 0.1, 1.5) + grab(827322, 2, 0.1, 1.5)
    click = grab(175678, 4, 0.03, 0.4)
    # ---- Mossmaul: grizzly roar and huff, a walrus-like bellow (elephant seal / sea lion) pitched down, the jaw-pop.
    griz = grab(519599, 4, 0.4, 3.0) + grab(763026, 2, 0.4, 3.0)
    run = grab(519600, 2, 0.3, 2.5)
    roar = grab(611721, 2, 0.6, 5.0)
    seal = grab(52204, 3, 0.5, 3.0, band=(60, 3000))
    sealg = grab(849186, 2, 0.3, 2.0)
    huff = grab(389631, 2, 0.1, 1.2) + grab(350414, 2, 0.3, 2.0)
    def pop(k): return pitch(click[k % 4], 0.55)
    def maul_roar(k, p=0.86):
        r = pitch(roar[k % 2], p)
        return mix(r, level_to(pitch(seal[k % 3], 0.7), r, -6), 0.08)
    voice("mossmaul", "idle", [fused(griz[i], moss, -14) for i in range(3)], [519599, 853612])
    voice("mossmaul", "aggro", [maul_roar(0), maul_roar(1, 0.8), mix(griz[4], level_to(pitch(seal[2], 0.72), griz[4], -5))],
          [611721, 52204, 763026])
    voice("mossmaul", "attack", [mix(head(griz[3 + i % 3], 0.8), level_to(claw[i], griz[3], -3), 0.1) for i in range(3)], [519599, 763026, 830466])
    voice("mossmaul", "hurt", [pitch(head(sealg[0], 0.7), 1.1), pitch(head(griz[5], 0.6), 1.12), pitch(head(run[0], 0.6), 1.1)],
          [849186, 763026, 519600])
    voice("mossmaul", "death", [tail_out(fused(pitch(roar[0], 0.66), moss, -10), 1.5), tail_out(pitch(seal[1], 0.6), 1.2)], [611721, 853612, 52204])
    hhp = [seq([pitch(huff[k % 4], 0.8), pitch(huff[(k + 1) % 4], 0.85), pop(k)], 0.42) for k in range(2)]
    voice("mossmaul", "tell", [glide(fused(h, moss, -12), 0, 3) for h in hhp], [389631, 350414, 175678, 853612])
    # the huff-huff-pop, then a roar that shakes needles off the branches
    sigm = []
    for k in range(2):
        r = maul_roar(k, 0.82)
        h = mix(hhp[k], r, len(hhp[k]) / SR + 0.05)
        sigm.append(mix(h, level_to(bandpass(leaves[k], lo=1500), r, -8), len(hhp[k]) / SR + 0.5))
    voice("mossmaul", "signature", sigm, [389631, 350414, 175678, 611721, 52204, 422963])

    # ---- Thornlynx: clouded-leopard snarl and chuff (snow leopard), Eurasian lynx yowl and huff, a cougar-like scream (fox
    # scream pitched down); bark scrape, leaf shower.
    lhuff = grab(844316, 2, 0.1, 1.5) + grab(611192, 2, 0.2, 2.0)
    chuff = grab(683661, 3, 0.1, 1.5)
    tig = grab(496131, 3, 0.4, 3.0)
    hiss = grab(455495, 3, 0.3, 3.0)
    scream = grab(634005, 4, 0.3, 2.5) + grab(676376, 2, 0.3, 2.5) + grab(554308, 2, 0.3, 2.5)
    troar = grab(263115, 2, 0.5, 4.0)
    voice("thornlynx", "idle", [fused(chuff[0], moss, -14), lhuff[0], fused(chuff[1], leaves[0], -16)], [683661, 844316, 853612, 422963])
    voice("thornlynx", "aggro", [mix(hiss[i], level_to(pitch(tig[i], 1.15), hiss[i], -4), 0.1) for i in range(3)], [455495, 496131])
    voice("thornlynx", "attack", [mix(pitch(head(tig[i], 0.6), 1.2), level_to(claw[i], tig[i], -2), 0.05) for i in range(3)], [496131, 830466])
    voice("thornlynx", "hurt", [pitch(head(lhuff[2], 0.6), 1.15), pitch(head(scream[0], 0.5), 0.95), pitch(head(lhuff[3], 0.5), 1.1)], [611192, 634005])
    voice("thornlynx", "death", [tail_out(pitch(scream[1], 0.75), 0.8), tail_out(pitch(troar[0], 1.1), 1.0)], [634005, 263115])
    voice("thornlynx", "tell", [glide(head(hiss[i], 1.0), 0, 4) for i in range(2)], [455495])
    # silence, then a rustle overhead: the drop has no voice until it lands (a leaf shower falling, the thump, the snarl)
    land = grab(853591, 2, 0.2, 3.0)
    siglx = []
    for k in range(2):
        rust = bandpass(leaves[(k + 1) % 3], lo=2000)
        s = mix(np.zeros((int(0.5 * SR), 1)), level_to(rust, rust, -6), 0.5)
        s = mix(s, level_to(land[k], rust, 4), len(s) / SR - 0.1)
        siglx.append(mix(s, level_to(pitch(head(tig[k], 0.7), 1.15), land[k], 0), len(s) / SR - 0.05))
    voice("thornlynx", "signature", siglx, [422963, 853591, 496131])

    # ---- Grey Matron (a Thornlynx matriarch): clouded-leopard chuff, cougar scream, snow-leopard yowl; claw on bark.
    voice("grey_matron", "idle", [fused(pitch(chuff[i], 0.85), claw[i], -10) for i in range(3)], [683661, 830466])
    voice("grey_matron", "aggro", [mix(pitch(scream[2 + i], 0.72), level_to(pitch(hiss[i], 0.85), scream[2 + i], -4), 0.2) for i in range(3)],
          [634005, 676376, 455495])
    voice("grey_matron", "attack", [mix(pitch(head(tig[i % 3], 0.7), 0.95), level_to(claw[2 + i], tig[i % 3], 0), 0.05) for i in range(4)],
          [496131, 830466, 827322])
    voice("grey_matron", "hurt", [pitch(head(scream[5 + i % 3], 0.6), 0.85) for i in range(3)], [676376, 554308])
    voice("grey_matron", "death", [tail_out(echoes(pitch(scream[1], 0.62)), 1.5), tail_out(pitch(troar[1], 0.85), 1.5)], [634005, 263115])
    voice("grey_matron", "tell", [fused(glide(pitch(head(hiss[i], 1.2), 0.85), 0, 4), claw[i], -8) for i in range(2)], [455495, 830466])
    # a scream from somewhere above you: high, thinned and echoing off the oaks, leaves falling after it
    voice("grey_matron", "signature", [mix(echoes(bandpass(pitch(scream[i], 0.8), lo=350), lp=4000), level_to(bandpass(leaves[i], lo=1500), scream[i], -10), 0.5)
                                       for i in range(2)], [634005, 422963])

    # ---- Rootcoil: python / cobra hiss, a soft bull-snake bellow (alligator growl pitched up), slow breathing; bark-on-bark
    # scale slide and branch creak.
    shiss = grab(540162, 2, 0.3, 2.0) + grab(553374, 2, 0.3, 2.5) + grab(560773, 3, 0.4, 3.0)
    ahiss = grab(675513, 2, 0.4, 2.0)
    bell = grab(527844, 3, 0.4, 2.5)
    creak = grab(726217, 4, 0.4, 3.0) + grab(456814, 1, 0.3, 2.0) + grab(130364, 2, 0.5, 3.0)
    breath = grab(350414, 4, 0.5, 3.0)
    slide = part(417910, 2.0)                            # bark ripping slowly: the scales sliding
    def logbreath(k):
        b = pitch(breath[k % 4], 0.62)
        return mix(fused(b, creak[k % 7], -12), level_to(bandpass(pitch(shiss[4 + k % 3], 0.7), hi=2500), b, -10), 0.3)
    voice("rootcoil", "idle", [logbreath(i) for i in range(3)], [350414, 726217, 560773])
    voice("rootcoil", "aggro", [mix(shiss[i], level_to(pitch(bell[i], 1.2), shiss[i], -6), 0.15) for i in range(3)], [540162, 553374, 527844])
    voice("rootcoil", "attack", [mix(head(ahiss[i % 2], 0.6), level_to(head(creak[i], 0.5), ahiss[i % 2], -2), 0.1) for i in range(3)], [675513, 726217])
    voice("rootcoil", "hurt", [pitch(head(shiss[i], 0.5), 1.15) for i in (1, 3, 5)], [540162, 553374, 560773])
    voice("rootcoil", "death", [tail_out(fused(pitch(breath[0], 0.5), slide, -8), 1.5), tail_out(fused(pitch(bell[2], 0.8), creak[5], -6), 1.0)],
          [350414, 417910, 527844, 130364])
    voice("rootcoil", "tell", [fused(glide(head(shiss[4 + i], 1.2), 0, 4), slide, -10) for i in range(2)], [560773, 417910])
    # the slow breathing of a log: a long, low exhale every 3 s with the bark creaking
    voice("rootcoil", "signature", [seq([logbreath(k), logbreath(k + 1)], 3.0) for k in range(2)], [350414, 726217, 560773])

    # ---- Bramblewasp: giant-hornet wingbeat, yellowjacket swarm, mandible clicks amplified; paper rustle.
    buzz = grab(392458, 4, 0.5, 3.0) + grab(856766, 2, 0.3, 2.0)
    hornet = part(623758, 4.0)
    swarm = part(798671, 5.0)
    mand = grab(573935, 6, 0.03, 0.5)
    paper = part(731484, 2.0)
    def clicks(k): return seq([pitch(mand[(k + j) % 6], 0.8 + 0.1 * j) for j in range(3)], 0.06)
    voice("bramblewasp", "idle", [fused(head(buzz[i], 1.5), paper, -16) for i in range(3)], [392458, 731484])
    voice("bramblewasp", "aggro", [ramp(mix(head(pitch(hornet, 1.0 + 0.06 * i), 1.6), level_to(clicks(i), hornet, -4), 0.3), 0.3, 1.0, 1.2) for i in range(3)],
          [623758, 573935])
    voice("bramblewasp", "attack", [mix(clicks(i), level_to(head(buzz[i + 1], 0.5), clicks(i), 0), 0.08) for i in range(3)], [573935, 392458])
    voice("bramblewasp", "hurt", [pitch(head(b, 0.4), 1.25) for b in buzz[3:6]], [392458, 856766])
    voice("bramblewasp", "death", [tail_out(glide(head(buzz[0], 1.2), 0, -7), 0.6), tail_out(glide(head(buzz[4], 1.0), 0, -5), 0.5)], [392458, 856766])
    voice("bramblewasp", "tell", [glide(head(pitch(hornet, 0.95), 1.0), 0, 4), glide(head(buzz[1], 1.0), 0, 5)], [623758, 392458])
    # the rise from a single buzz to a swarm roar when a nest is hit
    sigw = []
    for k in range(2):
        one = head(buzz[k], 1.0)
        s = mix(one, level_to(ramp(head(swarm, 3.0), 0.0, 1.0, 1.6), one, 6), 0.4)
        sigw.append(mix(s, level_to(head(paper, 1.0), one, -6), 0.4))
    voice("bramblewasp", "signature", sigw, [392458, 798671, 731484])

    # ---- Bramblequeen: giant-hornet wingbeat slowed, cicada screech, mandible clicks; the hive's paper roar.
    cic = part(272169, 3.0, band=(1500, 12000))
    proar = part(545532, 3.0)
    qbuzz = pitch(hornet, 0.6)
    voice("bramblequeen", "idle", [fused(head(pitch(hornet, 0.6 + 0.04 * i), 2.0), proar, -12) for i in range(3)], [623758, 545532])
    voice("bramblequeen", "aggro", [mix(head(qbuzz, 2.0), level_to(head(pitch(cic, 0.9 + 0.08 * i), 1.6), qbuzz, -3), 0.3) for i in range(3)], [623758, 272169])
    voice("bramblequeen", "attack", [mix(pitch(clicks(i), 0.7), level_to(head(proar, 0.8), clicks(i), -4), 0.1) for i in range(4)], [573935, 545532])
    voice("bramblequeen", "hurt", [mix(pitch(head(buzz[i], 0.5), 0.8), level_to(head(cic, 0.4), buzz[i], -6)) for i in range(3)], [392458, 272169])
    voice("bramblequeen", "death", [tail_out(glide(head(qbuzz, 3.0), 0, -9), 1.5), tail_out(fused(glide(head(cic, 2.0), 0, -6), proar, -4), 1.0)],
          [623758, 272169, 545532])
    voice("bramblequeen", "tell", [glide(mix(head(qbuzz, 1.2), level_to(head(cic, 1.2), qbuzz, -6)), 0, 3) for _ in range(2)], [623758, 272169])
    # the buzz pitch dropping before the sting, then the clicks
    voice("bramblequeen", "signature", [mix(glide(head(pitch(hornet, 0.75 - 0.05 * k), 1.6), 0, -6), level_to(clicks(k), hornet, 2), 1.5) for k in range(2)],
          [623758, 573935])

    # ---- Elderelk: wapiti bugle, red-stag roar, an Irish-elk scale (moose grunt pitched down); antler rattle, ivy swish.
    bugle = grab(250191, 1, 0.5, 3.0) + grab(826421, 3, 0.8, 4.0)
    stag = grab(757773, 4, 0.6, 3.5) + grab(653533, 2, 0.6, 3.5) + grab(653532, 2, 0.6, 3.5)
    moose = grab(486554, 3, 0.3, 3.0)
    antler = grab(866683, 1, 0.05, 1.0) + grab(59884, 3, 0.05, 1.0)
    ivy = part(445788, 2.0)
    voice("elderelk", "idle", [fused(pitch(moose[i], 0.85), ivy, -12) for i in range(3)], [486554, 445788])
    voice("elderelk", "aggro", [mix(bugle[0], level_to(stag[0], bugle[0], -5), 0.4), bugle[1], mix(stag[1], level_to(antler[0], stag[1], -6), 0.0)],
          [250191, 826421, 757773, 866683])
    voice("elderelk", "attack", [mix(antler[i % 4], level_to(head(moose[i], 0.5), antler[i % 4], -4), 0.02) for i in range(3)], [866683, 59884, 486554])
    voice("elderelk", "hurt", [pitch(head(stag[4], 0.6), 1.15), pitch(head(stag[6], 0.6), 1.2), pitch(head(moose[2], 0.5), 1.1)], [653533, 653532, 486554])
    voice("elderelk", "death", [tail_out(pitch(stag[2], 0.8), 1.2), tail_out(fused(pitch(moose[0], 0.7), ivy, -8), 0.8)], [757773, 486554, 445788])
    voice("elderelk", "tell", [glide(head(bugle[i], 1.2), 0, 3) for i in range(2)], [250191, 826421])
    # the bugle that turns a herd: the bugle over a stag roar, antlers rattling
    voice("elderelk", "signature", [mix(mix(bugle[k], level_to(pitch(stag[3 - k], 0.9), bugle[k], -4), 0.6), level_to(antler[k + 1], bugle[k], -6), 0.2)
                                    for k in range(2)], [250191, 826421, 757773, 59884])

    # ---- Hornwarden: red-stag roar, wapiti bugle, moose grunt (all pitched down, he is twice the size); antler clash,
    # banner rags flapping.
    rags = part(245839, 3.0)
    voice("hornwarden", "idle", [fused(pitch(moose[i], 0.7), rags, -10) for i in range(3)], [486554, 245839])
    voice("hornwarden", "aggro", [mix(pitch(stag[i], 0.75), level_to(pitch(bugle[i % 2], 0.8), stag[i], -4), 0.3) for i in range(3)], [757773, 250191, 826421])
    voice("hornwarden", "attack", [mix(pitch(antler[i % 4], 0.75), level_to(head(pitch(stag[4 + i % 3], 0.8), 0.6), antler[i % 4], -3), 0.03) for i in range(4)],
          [866683, 59884, 653533, 653532])
    voice("hornwarden", "hurt", [pitch(head(stag[5 + i % 3], 0.7), 0.95) for i in range(3)], [653533, 653532])
    voice("hornwarden", "death", [tail_out(fused(pitch(stag[0], 0.62), rags, -8), 2.0), tail_out(pitch(moose[1], 0.55), 1.5)], [757773, 245839, 486554])
    voice("hornwarden", "tell", [fused(glide(pitch(head(stag[i], 1.4), 0.75), 0, 3), rags, -10) for i in range(2)], [757773, 245839])
    # the bellow that sends the herd: roar into bugle, the antlers clashed, the rags snapping
    voice("hornwarden", "signature", [fused(mix(pitch(stag[k], 0.7), level_to(pitch(bugle[k], 0.75), stag[k], -2), 0.9), rags, -8) for k in range(2)],
          [757773, 250191, 826421, 245839])

    # ---- Burlbeast: gaur snort and bellow (bison), American-bison grunt, yak grunt; the wood-like knock of its back plates.
    bhuff = grab(867549, 3, 0.1, 1.5) + grab(873888, 2, 0.2, 2.0)
    bbell = grab(519594, 3, 0.6, 4.0) + grab(827111, 1, 0.4, 3.0)
    yak = grab(834331, 4, 0.2, 2.0)
    knock = grab(554916, 3, 0.05, 1.0) + grab(610007, 1, 0.05, 1.0)
    voice("burlbeast", "idle", [fused(yak[i], knock[i], -10) for i in range(3)], [834331, 554916])
    voice("burlbeast", "aggro", [mix(bbell[i], level_to(bhuff[i], bbell[i], -4), 0.0) for i in range(3)], [519594, 867549])
    voice("burlbeast", "attack", [mix(head(bhuff[3 + i % 2], 0.6), level_to(knock[i], bhuff[3 + i % 2], 2), 0.1) for i in range(3)], [873888, 554916, 610007])
    voice("burlbeast", "hurt", [pitch(head(yak[3], 0.6), 1.1), pitch(head(bhuff[0], 0.5), 1.15), pitch(head(bbell[3], 0.6), 1.1)], [834331, 867549, 827111])
    voice("burlbeast", "death", [tail_out(pitch(bbell[0], 0.75), 1.2), tail_out(fused(pitch(yak[1], 0.7), knock[3], -6), 0.8)], [519594, 834331, 610007])
    voice("burlbeast", "tell", [glide(head(pitch(bbell[i + 1], 0.85), 1.2), 0, 3) for i in range(2)], [519594])
    # a deep rolling bellow before the crash, the plates knocking as it lowers its head
    voice("burlbeast", "signature", [mix(seq([pitch(bbell[k], 0.75), pitch(bbell[k + 1], 0.7)], 0.9), level_to(seq([knock[0], knock[1], knock[2]], 0.18), bbell[k], -2), 1.4)
                                     for k in range(2)], [519594, 554916])

    # ---- Truffler: wild-boar grunt and squeal, peccary-like bark (boar growl), pig snuffle; soil rooting.
    boarg = grab(764944, 3, 0.3, 2.5) + grab(810908, 2, 0.3, 2.5)
    squeak = grab(425241, 3, 0.2, 2.0) + grab(147240, 2, 0.3, 2.0)
    sniff = grab(864329, 2, 0.1, 1.5) + grab(620079, 3, 0.2, 2.0)
    soil = part(696531, 2.5)
    voice("truffler", "idle", [fused(sniff[i], soil, -10) for i in range(3)], [864329, 620079, 696531])
    voice("truffler", "aggro", [mix(boarg[i], level_to(head(squeak[i], 0.6), boarg[i], -6), 0.3) for i in range(3)], [764944, 425241])
    voice("truffler", "attack", [fused(head(boarg[3 + i % 2], 0.7), soil, -10) for i in range(2)] + [head(sniff[3], 0.6)], [810908, 620079, 696531])
    voice("truffler", "hurt", [head(squeak[3], 0.6), head(squeak[4], 0.6), pitch(head(squeak[0], 0.5), 1.1)], [147240, 425241])
    voice("truffler", "death", [tail_out(pitch(squeak[1], 0.85), 0.8), tail_out(pitch(boarg[2], 0.8), 0.6)], [425241, 764944])
    voice("truffler", "tell", [glide(head(squeak[i], 0.8), 0, 4) for i in range(2)], [425241])
    # snuffle, snuffle, a sharp snort (a find, or a leaf pit under the leaves)
    voice("truffler", "signature", [fused(seq([sniff[k], sniff[k + 1], pitch(head(boarg[k], 0.4), 1.2)], 0.5), soil, -12) for k in range(2)],
          [864329, 620079, 764944, 696531])

    # ---- Hiveback: bumblebee drone pitched down, honeybee piping, carpenter-bee buzz; soft fur brush.
    bumble = part(202993, 4.0)
    bumble2 = grab(466795, 2, 0.4, 3.0)
    carp = grab(95050, 3, 0.4, 3.0)
    honey = part(846433, 2.5)
    hive = part(640514, 4.0)
    fur = part(807199, 2.5)
    voice("hiveback", "idle", [fused(head(pitch(bumble, 0.55 + 0.04 * i), 2.0), fur, -12) for i in range(3)], [202993, 807199])
    voice("hiveback", "aggro", [ramp(mix(pitch(carp[i], 0.6), level_to(head(pitch(honey, 0.8), 1.5), carp[i], -4), 0.1), 0.4, 1.0) for i in range(3)],
          [95050, 846433])
    voice("hiveback", "attack", [fused(head(pitch(bumble2[i % 2], 0.65), 0.7), fur, -6) for i in range(3)], [466795, 807199])
    voice("hiveback", "hurt", [pitch(head(c, 0.5), 0.9) for c in carp], [95050])
    voice("hiveback", "death", [tail_out(glide(head(pitch(bumble, 0.6), 2.0), 0, -6), 1.0), tail_out(fused(pitch(carp[0], 0.5), fur, -6), 0.8)],
          [202993, 95050, 807199])
    voice("hiveback", "tell", [glide(head(pitch(bumble, 0.6), 1.2), 0, 4), glide(head(pitch(hive, 0.7), 1.2), 0, 4)], [202993, 640514])
    # the hum rising to a heat-ball thrum
    voice("hiveback", "signature", [mix(ramp(glide(head(pitch(bumble, 0.55), 2.5), 0, 5), 0.3, 1.0), level_to(tremble(head(pitch(hive, 0.8), 2.5), 28, 0.7), bumble, -2), 0.5)
                                    for _ in range(2)], [202993, 640514])

    # ---- Fernsheep: Soay-like sheep bleat, goat bleat, ram grunt; fleece rustle.
    bleat = grab(210511, 3, 0.3, 2.0) + grab(428103, 3, 0.3, 2.0) + grab(609160, 3, 0.3, 2.0)
    goat = grab(581240, 2, 0.3, 2.0) + grab(200333, 2, 0.3, 2.0)
    ram = grab(186921, 3, 0.2, 2.0)
    lamb = grab(266689, 2, 0.3, 2.0)
    fleece = part(584866, 2.5)
    voice("fernsheep", "idle", [fused(bleat[i * 3], fleece, -14) for i in range(3)], [210511, 428103, 609160, 584866])
    voice("fernsheep", "aggro", [mix(ram[i], level_to(goat[i % 4], ram[i], -4), 0.3) for i in range(3)], [186921, 581240, 200333])
    voice("fernsheep", "attack", [fused(head(ram[i], 0.6), fleece, -6) for i in range(3)], [186921, 584866])
    voice("fernsheep", "hurt", [pitch(head(lamb[0], 0.6), 1.05), pitch(head(goat[2], 0.6), 1.1), pitch(head(bleat[1], 0.6), 1.15)], [266689, 200333, 210511])
    voice("fernsheep", "death", [tail_out(pitch(bleat[4], 0.8), 0.8), tail_out(pitch(goat[1], 0.8), 0.6)], [428103, 581240])
    voice("fernsheep", "tell", [glide(head(ram[i], 0.9), 0, 3) for i in range(2)], [186921])
    # flock bleats running in a wave
    voice("fernsheep", "signature", [chorus([bleat[0], bleat[3], goat[0], bleat[6], bleat[2], lamb[1]], [0, 0.3, 0.55, 0.85, 1.1, 1.4], [1.0, 1.05, 0.95, 1.08, 0.97, 1.0]),
                                     chorus([bleat[5], bleat[7], bleat[1], goat[3], bleat[8]], [0, 0.35, 0.6, 0.95, 1.25], [1.02, 0.96, 1.06, 1.0, 0.94])],
          [210511, 428103, 609160, 581240, 200333, 266689])

    # ---- Dappledoe: fallow-deer groan (a stag's groan pitched up), roe-deer bark, fawn bleat; hoof on leaf litter.
    rbark = grab(203593, 3, 0.15, 1.5) + grab(569926, 2, 0.15, 1.5) + grab(696776, 1, 0.05, 1.0)
    rcall = grab(724578, 3, 0.2, 2.0)
    groan = [pitch(s, 1.35) for s in stag[4:7]]
    fawn = [pitch(g, 1.3) for g in goat[2:4]]
    hoof = grab(426616, 4, 0.05, 0.8)
    voice("dappledoe", "idle", [fused(rcall[i], hoof[i], -10) for i in range(3)], [724578, 426616])
    voice("dappledoe", "aggro", [mix(rbark[i], level_to(head(groan[i], 0.8), rbark[i], -4), 0.25) for i in range(3)], [203593, 653533])
    voice("dappledoe", "attack", [mix(seq([hoof[i], hoof[i + 1]], 0.12), level_to(head(rbark[3 + i % 3], 0.4), hoof[i], -2), 0.05) for i in range(3)],
          [426616, 569926, 696776])
    voice("dappledoe", "hurt", [pitch(head(fawn[0], 0.5), 1.1), pitch(head(rbark[1], 0.4), 1.1), pitch(head(fawn[1], 0.5), 1.15)], [200333, 203593])
    voice("dappledoe", "death", [tail_out(pitch(groan[0], 0.9), 0.8), tail_out(pitch(fawn[0], 0.85), 0.5)], [653533, 200333])
    voice("dappledoe", "tell", [glide(head(rbark[i], 0.7), 0, 4) for i in range(2)], [203593])
    # the alarm bark and the bounding away through the litter
    voice("dappledoe", "signature", [mix(seq([rbark[k], rbark[k + 2]], 0.55), level_to(seq([hoof[(k + j) % 4] for j in range(5)], 0.16), rbark[k], -6), 1.0)
                                     for k in range(2)], [203593, 426616])

    # ---- Glimmerfinch: goldfinch twitter, hummingbird chirp and wing hum; tiny wing whirr.
    gold = grab(377064, 3, 0.2, 2.0) + grab(803350, 3, 0.2, 2.0) + grab(616983, 2, 0.2, 2.0)
    hum = grab(541223, 3, 0.05, 1.0) + grab(399440, 3, 0.05, 1.0)
    whirr = part(467294, 1.5)
    wing = part(194772, 2.0, band=(80, 2000))
    voice("glimmerfinch", "idle", [gold[0], gold[3], fused(gold[6], whirr, -12)], [377064, 803350, 616983, 467294])
    voice("glimmerfinch", "aggro", [mix(hum[i], level_to(head(whirr, 0.6), hum[i], -4), 0.0) for i in range(3)], [541223, 467294])
    voice("glimmerfinch", "attack", [fused(head(hum[3 + i], 0.4), wing, -6) for i in range(3)], [399440, 194772])
    voice("glimmerfinch", "hurt", [pitch(head(h, 0.3), 1.15) for h in hum[:3]], [541223])
    voice("glimmerfinch", "death", [tail_out(glide(head(gold[1], 0.8), 0, -4), 0.4), tail_out(fused(pitch(hum[4], 0.9), whirr, -6), 0.4)],
          [377064, 399440, 467294])
    voice("glimmerfinch", "tell", [glide(head(gold[i + 4], 0.8), 0, 4) for i in range(2)], [803350])
    # a chorus over a find
    voice("glimmerfinch", "signature", [fused(chorus([gold[0], gold[2], gold[4], gold[6], hum[1]], [0, 0.2, 0.45, 0.7, 0.9], [1.0, 1.06, 0.97, 1.03, 1.0]), wing, -14),
                                        fused(chorus([gold[1], gold[3], gold[5], gold[7], hum[4]], [0, 0.25, 0.5, 0.65, 0.95], [1.02, 0.98, 1.05, 1.0, 0.96]), wing, -14)],
          [377064, 803350, 616983, 541223, 399440, 194772])

    # ---- Old Gnarl (an awakened oak): timber stress cracks and groans, an elephant rumble pitched down, wind through a
    # crown; wood splitting, roots tearing from soil.
    tcreak = grab(620759, 4, 0.5, 4.0) + grab(140047, 2, 0.8, 5.0)
    crack = grab(669457, 2, 0.1, 1.5) + grab(354104, 2, 0.1, 2.0) + grab(183452, 1, 0.1, 1.5) + grab(501303, 1, 0.1, 1.5)
    ele = grab(818986, 3, 0.8, 5.0, band=(25, 900)) + grab(810176, 1, 0.4, 3.0)
    crown = part(523389, 4.0)
    split = grab(519567, 3, 0.1, 1.5)
    roots = grab(594401, 2, 0.2, 2.0) + grab(774142, 2, 0.2, 3.0)
    def groan_t(k, p=0.8):
        g = pitch(tcreak[k % 6], p)
        return mix(g, level_to(tail_out(head(pitch(ele[k % 4], 0.6), len(g) / SR), 0.5), g, -5), 0.1)   # the rumble ends with the groan
    voice("old_gnarl", "idle", [fused(groan_t(i), crown, -10) for i in range(3)], [620759, 818986, 523389])
    voice("old_gnarl", "aggro", [mix(groan_t(i + 3, 0.7), level_to(crack[i], groan_t(i + 3), 0), 0.2) for i in range(3)], [620759, 140047, 818986, 810176, 669457, 354104])
    voice("old_gnarl", "attack", [mix(pitch(crack[i], 0.8), level_to(head(split[i % 3], 0.6), crack[i], -2), 0.05) for i in range(4)], [669457, 354104, 183452, 519567])
    voice("old_gnarl", "hurt", [mix(pitch(crack[4 + i % 2], 0.9), level_to(head(tcreak[i], 0.6), crack[4 + i % 2], -4)) for i in range(3)], [183452, 501303, 620759])
    voice("old_gnarl", "death", [tail_out(fused(pitch(ele[0], 0.55), crown, -6), 2.5), tail_out(mix(groan_t(1, 0.6), level_to(roots[3], groan_t(1), -2), 1.0), 1.5)],
          [818986, 523389, 620759, 774142])
    voice("old_gnarl", "tell", [fused(glide(head(groan_t(i), 1.4), 0, 3), crown, -8) for i in range(2)], [620759, 818986, 523389])
    # the crack of a stump torn up before the boulder throw: roots tearing, the soil, then the crack
    voice("old_gnarl", "signature", [mix(mix(pitch(roots[k], 0.8), level_to(roots[2 + k], roots[k], 0), 0.15), level_to(pitch(crack[k], 0.75), roots[k], 3),
                                         len(roots[k]) / SR * 0.8) for k in range(2)], [594401, 774142, 669457])

    # ---- Sylvaroth, the World-Root: elephant infrasound, timber cracks, a humpback-like moan pitched down, red-stag roar;
    # wood splitting, roots tearing, wind through the lattice.
    whale = [tail_out(head(w, 3.0), 0.8) for w in grab(171945, 3, 1.0, 5.0, band=(30, 1500))]
    rumble = part(217657, 6.0, band=(25, 400))
    splint = grab(869103, 2, 0.3, 4.0)
    wind = part(104282, 4.0)
    def world_groan(k, p=0.5):
        w = pitch(whale[k % 3], p)
        w = mix(w, level_to(tail_out(head(pitch(ele[k % 3], 0.5), len(w) / SR - 0.2), 0.8), w, -3), 0.2)
        return mix(w, level_to(match(rumble, len(w)), w, -6))
    voice("sylvaroth", "idle", [fused(world_groan(i), wind, -10) for i in range(3)], [171945, 818986, 217657, 104282])
    voice("sylvaroth", "aggro", [mix(world_groan(i, 0.55), level_to(pitch(stag[i], 0.5), world_groan(i), -3), 0.6) for i in range(3)],
          [171945, 818986, 757773, 217657])
    voice("sylvaroth", "attack", [mix(pitch(crack[i % 6], 0.6), level_to(head(pitch(splint[i % 2], 0.8), 1.0), crack[i % 6], -2), 0.05) for i in range(4)],
          [669457, 354104, 183452, 501303, 869103])
    voice("sylvaroth", "hurt", [mix(pitch(crack[i], 0.7), level_to(head(pitch(tcreak[i], 0.6), 0.9), crack[i], -2), 0.05) for i in range(3)],
          [669457, 354104, 620759])
    voice("sylvaroth", "death", [tail_out(mix(world_groan(0, 0.42), level_to(splint[0], whale[0], -2), 1.5), 3.0),
                                 tail_out(fused(pitch(roots[3], 0.6), rumble, -4), 2.0)], [171945, 818986, 217657, 869103, 774142])
    voice("sylvaroth", "tell", [fused(glide(head(world_groan(i), 1.6), 0, 3), wind, -8, shape="rise") for i in range(2)], [171945, 818986, 104282])
    # a deep groan from under the clearing (you cross the run-back stone), then the roots tearing as it rises
    voice("sylvaroth", "signature", [mix(bandpass(world_groan(k, 0.45), hi=900), level_to(mix(pitch(roots[2 + k], 0.6), level_to(splint[k], roots[2 + k], -2), 0.3),
                                         whale[k], 0), 2.2) for k in range(2)], [171945, 818986, 217657, 774142, 869103])
    json.dump(VOICE, open(os.path.join(WORK, "forest_voices.json"), "w"))
    save_manifest()

# ================================================================ events (hazards, places, the bosses' moments, the Nature path)
EVENTS = {}
def event(key, clips, sources, folder="world"):
    """key 'forest.thorn_scrape' -> world/forest_thorn_scrape_1..n (folder 'spell' for the Nature path)."""
    files = []
    stem = key.replace(".", "_")
    for i, c in enumerate(clips):
        f = f"{folder}/{stem}_{i + 1}"
        write(f, c, "sfx", sources)
        files.append(f)
    EVENTS[key] = files

def loop_event(key, x, sources, target=-20.0):
    f = "world/" + key.replace(".", "_") + "_loop"
    write_loop(f, x, sources, target)
    EVENTS[key] = [f]

if want("events"):
    leaves = grab(422963, 3, 0.3, 4.0)
    twigs = grab(707868, 3, 0.05, 1.0) + grab(452570, 1, 0.05, 1.0) + grab(734627, 2, 0.1, 1.5)
    bush = grab(764900, 2, 0.2, 2.0) + grab(853612, 2, 0.3, 2.5)
    crack = grab(669457, 2, 0.1, 1.5) + grab(354104, 2, 0.1, 2.0) + grab(183452, 1, 0.1, 1.5) + grab(501303, 1, 0.1, 1.5)
    creak = grab(726217, 4, 0.4, 3.0) + grab(130364, 2, 0.5, 3.0) + grab(726921, 2, 0.3, 2.5)
    rumble = part(217657, 4.0, band=(25, 500))
    # -- thornbrake: a bramble dragging across you (bush and twig scrape), and the brake burning
    event("forest.thorn_scrape", [fused(head(bush[i], 0.8), twigs[i], -4) for i in range(3)], [764900, 853612, 707868, 452570])
    fireb = part(564621, 3.0)
    event("forest.thorn_burn", [mix(head(fireb, 2.5), level_to(seq([twigs[(i + j) % 6] for j in range(4)], 0.45), fireb, -6), 0.3) for i in range(2)],
          [564621, 707868, 734627])
    # -- widowmaker: the crack and creak up in the crown (the 1.0 s tell, rising), the branch coming down
    event("forest.widow_crack", [ramp(mix(head(creak[i], 1.0), level_to(crack[i], creak[i], 2), 0.85), 0.4, 1.0) for i in range(3)], [726217, 669457, 354104])
    fall = grab(153156, 1, 0.2, 2.0) + grab(695978, 2, 0.3, 3.0)
    thud = grab(853591, 2, 0.2, 3.0)
    event("forest.widow_fall", [mix(f, level_to(thud[i % 2], f, 2), max(0.0, len(f) / SR - 0.35)) for i, f in enumerate(fall)], [153156, 695978, 853591])
    # -- leaf pits: the hollow drum underfoot, then the collapse into the dark
    hollow = grab(554916, 3, 0.05, 1.0) + grab(364704, 1, 0.05, 1.0)
    crunch = grab(426616, 3, 0.05, 0.8)
    event("forest.leafpit_drum", [mix(crunch[i], level_to(bandpass(pitch(hollow[i], 0.7), hi=900), crunch[i], 0), 0.02) for i in range(3)], [426616, 554916, 364704])
    event("forest.leafpit_fall", [mix(mix(leaves[i], level_to(twigs[i + 2], leaves[i], 0), 0.1), level_to(pitch(thud[i], 0.8), leaves[i], 2), 0.9) for i in range(2)],
          [422963, 707868, 734627, 853591])
    # -- Bramblewasp nests: the hum (a loop, within 15 m), the swarm, the scout's mark
    loop_event("forest.wasp_hum", mix(part(435589, 8.0), level_to(part(201502, 8.0), part(435589, 8.0), -4)), [435589, 201502], -22.0)
    swarm = part(798671, 4.0)
    sw2 = part(164486, 4.0)
    event("forest.wasp_swarm", [ramp(mix(head(swarm, 3.0), level_to(head(sw2, 3.0), swarm, -3)), 0.4, 1.0, 0.8),
                                ramp(mix(head(sw2, 3.0), level_to(head(part(623134, 3.0), 3.0), sw2, -3)), 0.4, 1.0, 0.8)], [798671, 164486, 623134])
    zip_ = grab(392458, 2, 0.5, 2.0) + grab(856766, 1, 0.3, 2.0)
    mand = grab(573935, 4, 0.03, 0.5)
    event("forest.scout_mark", [mix(glide(head(z, 0.9), 3, -3), level_to(seq([mand[(i + j) % 4] for j in range(3)], 0.06), z, 0), 0.5) for i, z in enumerate(zip_)],
          [392458, 856766, 573935])
    # -- the Hiveback heat-ball: the bees' thrum swelling round a wasp
    hive = part(640514, 3.0)
    event("forest.heat_ball", [ramp(tremble(mix(head(pitch(hive, 0.85 + 0.08 * i), 2.5), level_to(head(pitch(part(341639, 2.5), 0.9), 2.5), hive, -3)), 26 + 4 * i, 0.5), 0.3, 1.0)
                               for i in range(2)], [640514, 341639])
    # -- fords in spate (a loop at the water) and the Great Falls (a burst of the plunge)
    loop_event("forest.spate", mix(part(588180, 8.0), level_to(part(436576, 8.0), part(588180, 8.0), -3)), [588180, 436576], -18.0)
    event("forest.falls", [head(part(650699, 3.0), 3.0), head(part(632107, 3.0), 3.0)], [650699, 632107])
    # -- the village lift: rope creaking through a pulley (a loop while it moves; the same creak as it settles)
    loop_event("forest.lift_creak", mix(part(862995, 4.0), level_to(part(547862, 4.0), part(862995, 4.0), -4)), [862995, 547862], -20.0)
    # -- the village bell, the Thrumming Root's pulse
    bellb = [peaks(857937, 1, 5.0, pre=0.05)[0], peaks(668647, 1, 5.0, pre=0.05)[0], peaks(454855, 1, 5.0, pre=0.05)[0]]
    event("forest.bell", [tail_out(b, 1.5) for b in bellb], [857937, 668647, 454855])
    hb = grab(249716, 4, 0.3, 1.5)
    event("forest.root_pulse", [mix(seq([bandpass(pitch(hb[k], 0.6), hi=300), bandpass(pitch(hb[k + 1], 0.6), hi=300)], 1.0), level_to(head(rumble, 2.5), hb[k], -6))
                                for k in range(2)], [249716, 217657])
    # -- an elder (a giant old tree) cracking and coming down
    big = grab(494071, 1, 1.0, 8.0) + grab(486263, 1, 1.0, 8.0) + grab(515313, 1, 1.0, 8.0) + grab(637912, 1, 1.0, 8.0)
    event("forest.elder_crack", [ramp(mix(pitch(head(creak[i], 1.4), 0.8), level_to(crack[2 + i], creak[i], 2), 1.1), 0.4, 1.0) for i in range(2)],
          [726217, 354104, 183452])
    event("forest.elder_fall", [tail_out(head(b, 6.0), 1.5) for b in big[:3]], [494071, 486263, 515313])
    # -- the Bloom's warning (2D): the birds rising all at once, a chime shimmer and a singing bowl
    dawn = part(611453, 5.0)
    bowl = peaks(352611, 1, 5.0, pre=0.05)[0]
    chim = grab(437337, 1, 1.0, 5.0)[0]
    event("forest.bloom_warn", [mix(mix(ramp(head(dawn, 5.0), 0.1, 1.0, 0.7), level_to(bowl, dawn, -2), 0.2), level_to(chim, dawn, -6), 0.8)], [611453, 352611, 437337])
    # -- the canopy (a gust through the crowns), a resting Rootcoil's breath, the Mossmaul's big roar
    event("forest.canopy_rustle", [head(part(sid, 3.0), 3.0) for sid in (575645, 523389, 865353)], [575645, 523389, 865353])
    br = grab(350414, 3, 0.5, 3.0)
    event("forest.log_breath", [fused(pitch(br[i], 0.6), creak[4 + i], -10) for i in range(2)], [350414, 130364])
    roar = grab(611721, 2, 0.6, 5.0)
    seal = grab(52204, 2, 0.5, 3.0, band=(60, 3000))
    event("forest.mossmaul_roar_big", [mix(mix(pitch(roar[i], 0.8), level_to(pitch(seal[i], 0.68), roar[i], -5), 0.08), level_to(bandpass(leaves[i], lo=1500), roar[i], -6), 0.6)
                                       for i in range(2)], [611721, 52204, 422963])
    # -- the Wild Hunt: hooves thundering past, the horn
    hooves = [head(part(479742, 4.0), 4.0), head(part(574317, 4.0), 4.0), head(part(583261, 4.0), 4.0)]
    event("forest.hunt_hooves", hooves, [479742, 574317, 583261])
    event("forest.hunt_horn", grab(69206, 2, 0.8, 6.0) + grab(148645, 1, 0.5, 4.0), [69206, 148645])
    # -- Sylvaroth's arena: a Root Knot breaking, a healing line regrowing, the Uprooting, vines creeping, seed pods
    rootsx = grab(774142, 2, 0.2, 3.0) + grab(594401, 2, 0.2, 2.0)
    wbreak = grab(520720, 1, 0.2, 2.0) + grab(794523, 1, 0.1, 2.0) + grab(675900, 1, 0.3, 4.0)
    event("forest.knot_break", [mix(pitch(w, 0.75), level_to(rootsx[i], w, -2), 0.1) for i, w in enumerate(wbreak)], [520720, 794523, 675900, 774142, 594401])
    grow = part(648958, 3.0)
    ice = part(735401, 3.0)
    event("forest.line_regrow", [ramp(fused(head(grow, 2.0), head(ice, 2.0), -4), 0.2, 1.0), ramp(fused(head(pitch(grow, 0.85), 2.0), head(creak[6], 2.0), -8), 0.2, 1.0)],
          [648958, 735401, 726921])
    event("forest.uproot", [mix(mix(head(rumble, 4.0), level_to(pitch(rootsx[i], 0.6), rumble, 2), 0.4), level_to(pitch(crack[i], 0.6), rumble, 4), 1.6) for i in range(2)],
          [217657, 774142, 669457, 354104])
    rust = part(523776, 2.5)
    event("forest.vine_creep", [fused(head(pitch(grow, 0.9 + 0.1 * i), 2.0), rust, -6) for i in range(2)], [648958, 523776])
    pods = grab(610143, 3, 0.05, 1.5)
    event("forest.seed_pod", [mix(p, level_to(head(bush[i], 0.4), p, -8), 0.0) for i, p in enumerate(pods)], [610143, 764900])
    # -- Old Gnarl's bark plates: stripped off, growing back
    rip = grab(417910, 2, 0.2, 2.0) + grab(846049, 1, 0.2, 2.0) + grab(847211, 1, 0.2, 2.0)
    event("forest.bark_strip", [mix(r, level_to(crack[i], r, -4), 0.2) for i, r in enumerate(rip[:3])], [417910, 846049, 847211, 669457, 354104])
    event("forest.bark_regrow", [ramp(fused(head(rev(rip[3]), 1.5), head(grow, 1.5), -2), 0.2, 1.0), ramp(fused(head(grow, 1.5), head(creak[7], 1.5), -6), 0.2, 1.0)],
          [847211, 648958, 726921])
    # -- the Bramblequeen's brood combs breaking (paper and larvae)
    tear = grab(870033, 1, 0.2, 2.0) + grab(489968, 1, 0.1, 1.5) + grab(644045, 1, 0.1, 2.0)
    larv = part(779173, 1.5)
    event("forest.comb_break", [fused(t, larv, -6) for t in tear], [870033, 489968, 644045, 779173])
    # -- the Grey Matron's Bower: a walkway coming down, a rope anchor cut, her kits' scream
    wcol = grab(675900, 1, 0.3, 4.0) + grab(675967, 1, 0.5, 5.0) + grab(794523, 1, 0.1, 2.0)
    event("forest.walkway_fall", [mix(w, level_to(leaves[i % 3], w, -6), 0.2) for i, w in enumerate(wcol)], [675900, 675967, 794523, 422963])
    ropec = grab(540266, 1, 0.05, 1.0) + grab(491960, 1, 0.05, 1.0)
    event("forest.anchor_cut", [mix(r, level_to(head(part(547862, 1.0), 0.6), r, -6), 0.0) for r in ropec], [540266, 491960, 547862])
    kit = grab(634005, 2, 0.3, 2.0) + grab(676376, 1, 0.3, 2.0)
    event("forest.kit_scream", [pitch(head(k, 1.2), 1.3) for k in kit], [634005, 676376])
    # -- a shrine offering (a singing bowl and a chime), digging up a cache
    event("forest.offering", [mix(head(bowl, 4.0), level_to(grab(398496, 1, 0.3, 3.0)[0], bowl, -4), 0.3),
                              mix(head(pitch(bowl, 1.12), 4.0), level_to(chim, bowl, -8), 0.2)], [352611, 398496, 437337])
    dig = grab(384362, 2, 0.2, 1.5) + grab(696531, 1, 0.2, 1.5) + grab(871629, 1, 0.2, 1.5)
    event("forest.dig", dig[:3], [384362, 696531, 871629])
    json.dump(EVENTS, open(os.path.join(WORK, "forest_events.json"), "w"))
    save_manifest()

# ================================================================ the Nature path (cook 2.17): spell/nature_*
if want("nature"):
    rust = grab(523776, 3, 0.2, 2.0) + grab(437356, 2, 0.3, 2.5)
    grow = part(648958, 3.0)
    grow2 = part(735401, 3.0)
    bowl = peaks(352611, 1, 5.0, pre=0.05)[0]
    chimes = grab(437337, 3, 0.3, 3.0) + grab(398496, 1, 0.3, 3.0)
    whip = grab(615761, 1, 0.05, 1.0) + grab(794818, 2, 0.1, 1.5)
    blow = grab(187707, 1, 0.05, 1.0)
    whoosh = grab(234915, 2, 0.2, 2.0)
    rope = part(547862, 2.0)
    vines = part(862995, 2.0)
    roots = grab(594401, 2, 0.2, 2.0) + grab(774142, 2, 0.2, 3.0) + grab(387083, 1, 0.05, 1.0)
    rumble = part(217657, 4.0, band=(25, 500))
    creak = grab(620759, 3, 0.5, 3.0)
    crack = grab(669457, 2, 0.1, 1.5) + grab(183452, 1, 0.1, 1.5)
    impact = grab(550918, 3, 0.1, 2.0)
    pods = grab(610143, 2, 0.05, 1.5)
    bark = grab(846049, 2, 0.2, 2.0)
    N = lambda key, clips, src: event("nature." + key, clips, src, folder="spell")
    # the cast: leaves gathering in the hand, a growth creak rising under them
    N("cast", [ramp(fused(head(rust[i], 1.0), head(glide(grow, 0, 3), 1.0), -4), 0.3, 1.0, 1.2) for i in range(3)], [523776, 437356, 648958])
    # a thorn flying: the dart's puff and a thin whip through the air
    N("thorn", [mix(blow[0], level_to(head(whip[i % 3], 0.4), blow[0], -4), 0.02) for i in range(3)], [187707, 615761, 794818])
    # roots grabbing a foe: rope-like tension, vines creaking, leaves
    N("entangle", [mix(fused(head(rope, 1.4), head(vines, 1.4), -3), level_to(head(rust[i], 0.8), rope, -6), 0.0) for i in range(3)], [547862, 862995, 523776, 437356])
    # Rooted: the feet taking root (roots tearing reversed: into the soil), a low ground thud
    N("rooted", [mix(rev(head(roots[i], 1.0)), level_to(head(rumble, 1.0), roots[i], -4), 0.4) for i in range(2)], [594401, 774142, 217657])
    # Thorned skin pricking an attacker: bark crunch and twigs
    N("thorned_skin", [mix(head(bark[i % 2], 0.6), level_to(head(whip[1 + i % 2], 0.3), bark[i % 2], -6), 0.05) for i in range(3)], [846049, 794818])
    # the pollen cloud round a Bloom plant (a loop while it hangs there: WRPlantSummon StartLoop): bees working the
    # flowers, a soft rustle, now and then a high chime glint
    hum = part(260787, 8.0)
    pol = sprinkle(stereo(mix(hum, level_to(bandpass(part(523776, 8.0), hi=4000), hum, -8))), [bandpass(pitch(c, 1.3), lo=2000) for c in chimes], 3.0, -14, 41)
    write_loop("spell/nature_pollen_loop", pol, [260787, 523776, 437337, 398496], -24.0)
    EVENTS["nature.pollen"] = ["spell/nature_pollen_loop"]
    # purifying: a singing bowl over the leaves
    N("purify", [mix(head(pitch(bowl, 1.0 + 0.06 * i), 3.0), level_to(head(rust[3 + i % 2], 1.2), bowl, -10), 0.1) for i in range(2)], [352611, 437356])
    # the World tree springing up: a huge creak and growth, the crown rustling out, a bowl under it
    N("world_tree", [mix(mix(pitch(head(creak[i], 2.5), 0.75), level_to(head(glide(grow, 0, 4), 2.5), creak[i], -2), 0.0), level_to(head(bowl, 3.0), creak[i], -8), 0.5)
                     for i in range(2)], [620759, 648958, 352611])
    # the Ancient guardian: rising out of the ground (roots, soil, timber), its slam
    N("guardian_rise", [mix(mix(head(rumble, 3.0), level_to(pitch(roots[2 + i], 0.7), rumble, 2), 0.3), level_to(pitch(crack[i], 0.7), rumble, 2), 1.4) for i in range(2)],
      [217657, 774142, 669457])
    N("guardian_slam", [mix(pitch(im, 0.75), level_to(head(rumble, 1.5), im, -3), 0.0) for im in impact], [550918, 217657])
    # healing: chimes over a soft growth
    N("heal", [fused(head(chimes[i], 1.6), head(grow2, 1.6), -10) for i in range(3)], [437337, 398496, 735401])
    # the Sprout turret's seed shot
    N("turret_shot", [mix(p, level_to(blow[0], p, -2), 0.0) for p in pods] + [pitch(blow[0], 0.9)], [610143, 187707])
    # a vine lash: the heavy swish and a crack
    N("vine_lash", [mix(whip[1 + i % 2], level_to(whip[0], whip[1 + i % 2], -2), 0.18) for i in range(3)], [794818, 615761])
    json.dump(EVENTS, open(os.path.join(WORK, "forest_nature.json"), "w"))
    save_manifest()

# ================================================================ ambient beds (~60 s loops; cook 3.4)
BEDS = {}
def bed(name, x, sources):
    write(f"amb/forest_{name}", loop_xfade(x, 4.0), "bed", sources, q=1)
    BEDS[name] = f"amb/forest_{name}"

if want("beds"):
    # the day birds (thinning in the deep wood by weight): a morning wood with blackbird, song thrush, wren and woodpecker
    morn = bed_src(844605)
    birds = grab(725332, 4, 0.8, 4.0) + grab(785669, 3, 0.8, 4.0) + grab(853671, 2, 0.8, 4.0) + grab(125423, 2, 0.5, 4.0)
    peck = grab(428146, 2, 0.3, 3.0)
    bed("day", sprinkle(sprinkle(morn, birds, 6, -4, 1), peck, 18, -10, 2), [844605, 725332, 785669, 853671, 125423, 428146])
    # the night: crickets and a still wood, tawny owls calling (loudest in the deep wood)
    nf = bed_src(808690); cr = bed_src(704381)
    owls = grab(745208, 3, 0.4, 3.0) + grab(867131, 3, 0.4, 3.0)
    bed("night", sprinkle(mix(nf, level_to(bandpass(cr, lo=2000), nf, -6)), owls, 10, -4, 3), [808690, 704381, 745208, 867131])
    # F1 Meadows: wind through the hay; by day skylarks and bees over it
    bed("meadows", mix(bed_src(639459), level_to(bed_src(517995), bed_src(639459), -4)), [639459, 517995])
    lark = bed_src(277149); bees = bed_src(260787)
    bed("meadows_day", mix(mix(lark, level_to(bed_src(399221), lark, -4)), level_to(bees, lark, -8)),
        [277149, 399221, 260787])
    # F2 Streams: brooks over stones; at night the frogs along the Alder
    brook = bed_src(454155)
    bed("streams", mix(brook, level_to(bed_src(530205), brook, -4)), [454155, 530205])
    bed("streams_night", mix(bed_src(810907), level_to(bed_src(687212), bed_src(810907), -4)), [810907, 687212])
    # F3 Villages: sheep bells on the commons, a cowbell, church bells far off, the wind in the hedges
    vil = mix(bandpass(bed_src(188288), hi=3000), level_to(bed_src(450370), bed_src(188288), -2))
    far = [bandpass(c, hi=2500) for c in grab(266525, 2, 1.0, 6.0) + grab(244517, 1, 1.0, 6.0)]
    sheep = [bandpass(s, hi=4000) for s in grab(609160, 3, 0.3, 2.0)]
    bed("villages", sprinkle(sprinkle(vil, far, 20, -10, 4), sheep, 12, -14, 5), [188288, 450370, 266525, 244517, 609160])
    # F4 Old-Growth: deep wind in the old crowns, trunks creaking, a woodpecker far off
    og = mix(bed_src(563571), level_to(bed_src(140047), bed_src(563571), -4))
    bed("oldgrowth", sprinkle(og, [bandpass(p, hi=4000) for p in peck], 16, -14, 6), [563571, 140047, 428146])
    # F5 the Elder Canopy: the canopy rushing high overhead, branches creaking
    can = mix(bed_src(523389), level_to(bed_src(706661), bed_src(523389), -4))
    bed("canopy", sprinkle(can, grab(726921, 2, 0.3, 2.5) + grab(726217, 2, 0.4, 3.0), 9, -10, 7), [523389, 706661, 726921, 726217])
    # the Hollow Root: drips in the dark, a low heartbeat in the roots, the ground's rumble
    drips = bed_src(818888); drips2 = bed_src(177958)
    heart = match(stereo(bandpass(seq([pitch(c, 0.75) for c in grab(249716, 4, 0.3, 1.5)] * 2, 1.25), hi=200)), len(drips))
    bed("hollow", mix(mix(drips, level_to(drips2, drips, -4)), level_to(heart, drips, -8)), [818888, 177958, 249716])
    # the Great Falls: the roar rising toward the scarp (the director fades it by distance)
    fall = bed_src(650699); fall2 = bed_src(632107)
    bed("falls", mix(fall, level_to(bandpass(fall2, hi=1500), fall, -4)), [650699, 632107])
    json.dump(BEDS, open(os.path.join(WORK, "forest_beds.json"), "w"))
    save_manifest()
