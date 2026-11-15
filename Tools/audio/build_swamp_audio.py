# Swamp (the Mire) sound pass: builds the biome's music, creature voices, hazard / Blood path events and ambient beds
# into Data/Audio from the sources in sources_swamp.py (Kevin MacLeod / incompetech music, Freesound CC0 / CC-BY
# recordings). The Storm peaks pass's rules (build_storm_audio.py): short sounds by their loudest 400 ms (-16 LUFS),
# beds by integrated loudness (-23), music by integrated loudness per the cook's 2.16 mix (-18 explore and combat,
# -23 the calm cues, -15 the boss), true-ish peak under -1 dBFS, beds loop with an equal-power crossfade.
# Creature calls are cut from real animal recordings and fused in numpy (pitch by resampling, like asetrate; each
# family's material layer mixed under the call), as cooks/biome-swamp.md 2.15 asks: 3+ real calls per family, tells
# rising in pitch. ffmpeg decodes and encodes (Vorbis q3 music, q4 effects, q1 beds: the pass stays near 40 MB).
# Usage: python build_swamp_audio.py <work> <Data/Audio> [music|voices|events|blood|beds ...]
#   <work>/dl/music/<Title>.mp3 (incompetech), <work>/dl/fs/<freesound id>.mp3 (Freesound HQ previews)
# Needs numpy, scipy, soundfile and ffmpeg (FFMPEG env var or on PATH). Writes <work>/swamp_manifest.json for credits.
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
mpath = os.path.join(WORK, "swamp_manifest.json")
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
    # file, incompetech title, start s, max s (the director plays explore pieces <= 200 s), loop (crossfade), loudness.
    # Cook 2.16 mix: -18 LUFS explore / combat / minis, calm cues -23 (night, the Coven, camp), the boss -15.
    ("swamp_explore_a", "SCP-x6x (Hopes)", 0, 200, False, -18.0),
    ("swamp_explore_a2", "Lost Time", 0, 200, False, -18.0),
    ("swamp_explore_b", "Virtutes Instrumenti", 0, 200, False, -18.0),
    ("swamp_explore_b2", "Intrepid", 0, 190, False, -18.0),
    ("swamp_explore_c", "Ghost Story", 0, 200, False, -18.0),
    ("swamp_explore_c2", "Past the Edge", 0, 90, False, -18.0),
    ("swamp_night_a", "Virtutes Vocis", 0, 180, False, -23.0),
    ("swamp_night_b", "Heart of Nowhere", 0, 175, False, -23.0),
    ("swamp_combat", "Wretched Destroyer", 0, 180, False, -18.0),
    ("swamp_mini", "Shadowlands 7 - Codex", 0, 180, False, -18.0),
    ("swamp_hex_warden", "Baba Yaga", 0, 180, False, -18.0),
    ("swamp_bloat", "SCP-x4x (Mind Leech)", 0, 180, False, -18.0),
    # Vharra: three sections, one per phase (P1 the march, P2 the flood's choir, P3 everything in), each looping.
    ("swamp_vharra_p1", "Crusade - Heavy Industry", 0, 170, True, -15.0),
    ("swamp_vharra_p2", "Final Battle of the Dark Wizards", 20, 160, True, -15.0),
    ("swamp_vharra_p3", "Clash Defiant", 70, 160, True, -15.0),
    ("swamp_sunken_coven", "Ossuary 6 - Air", 0, 200, False, -23.0),
    ("swamp_red_tide", "Controlled Chaos", 90, 110, True, -20.0),
    ("swamp_camp", "Trio for Piano, Cello, and Clarinet", 0, 120, False, -23.0),
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
        write("music/" + name, x, lufs, ["km:" + title])
    save_manifest()

# ================================================================ layers shared by the voices and events
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
    """A surface shivering: the layer's level trembles at hz (the water on Murkjaw's / Mother Maw's back)."""
    t = np.arange(len(x)) / SR
    return x * (1 - depth * 0.5 * (1 + np.sin(2 * np.pi * hz * t)))[:, None]

def echoes(x, taps=((0.23, -7), (0.47, -12), (0.74, -17), (1.05, -23)), lp=1800):
    """Taps off the sinkhole's walls: each later and duller."""
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

# ================================================================ creature voices (cook 2.15 recipes)
# Kinds: idle, aggro, attack, hurt, death (WRSound::Voice), tell (the wind-up, rising in pitch) and signature (the
# cook's signature moment). Every family: 3+ real animal calls and a material layer.
if want("voices"):
    # ---- Murkjaw: croc / alligator bellow, alligator hiss, bullfrog drone pitched down; water slap, duckweed tearing.
    bell = [tail_out(head(b, 2.8), 0.6) for b in grab(272408, 3, 1.0, 4.0, band=(40, 900))]
    croc = grab(403373, 3, 0.3, 2.0)
    growl = grab(527844, 4, 0.4, 2.5)
    hiss = grab(675513, 2, 0.4, 2.0)
    frog = grab(188723, 2, 0.4, 2.0, band=(60, 1200))
    slap = grab(545593, 2, 0.1, 1.0) + grab(679390, 1, 0.1, 1.0)
    splash = grab(442773, 2, 0.3, 3.0)
    weed = part(737308, 2.0)
    lap = part(827295, 6.0)
    def murk_bellow(b, i, p=0.86):
        x = pitch(b, p)
        return mix(x, level_to(pitch(frog[i % 2], 0.5), x, -7), 0.05)
    voice("murkjaw", "idle", [fused(g, weed, -16) for g in growl[:3]], [527844, 737308])
    voice("murkjaw", "aggro", [murk_bellow(b, i) for i, b in enumerate(bell)], [272408, 188723])
    voice("murkjaw", "attack", [mix(head(hiss[i % 2], 0.7), level_to(slap[i], hiss[i % 2], 0), 0.12) for i in range(3)],
          [675513, 545593, 679390])
    voice("murkjaw", "hurt", [fused(head(c, 0.9), slap[i], -8) for i, c in enumerate(croc)], [403373, 545593, 679390])
    voice("murkjaw", "death", [tail_out(mix(pitch(bell[0], 0.72), level_to(splash[0], bell[0], -6), 0.6), 1.2),
                               tail_out(fused(pitch(growl[3], 0.8), weed, -12), 0.8)], [272408, 442773, 527844, 737308])
    voice("murkjaw", "tell", [fused(glide(pitch(hiss[i], 0.9), 0, 4), pitch(frog[i], 0.5), -6) for i in range(2)], [675513, 188723])
    # the bellow under the surface that makes the water shiver, then the burst
    sig = []
    for i in range(2):
        b = murk_bellow(bell[i], i, 0.78)
        sh = tremble(bandpass(match(lap, len(b)), 200, 6000), 17 + 3 * i, 0.8)
        b = mix(b, level_to(sh, b, -4))
        sig.append(mix(b, level_to(splash[i], b, 2), len(b) / SR - 0.15))
    voice("murkjaw", "signature", sig, [272408, 188723, 827295, 442773])

    # ---- Gorgeleech: lamprey suction clicks, slug slime, elephant-seal gurgle pitched up; wet suck and pop.
    clicks = grab(175678, 4, 0.03, 0.4)
    pops = grab(444791, 4, 0.05, 0.6) + grab(386885, 2, 0.05, 0.8)
    gurg = grab(52204, 5, 0.5, 2.5, band=(80, 3000))
    slime = grab(666969, 3, 0.2, 1.5) + grab(433840, 2, 0.2, 1.5)
    suck = part(707321, 2.5)
    suck2 = part(707322, 2.5)
    def yclick(k):
        return seq([pitch(clicks[(k + j) % 4], [0.9, 1.0, 1.12][j]) for j in range(3)], 0.09)
    voice("gorgeleech", "idle", [fused(slime[i], pitch(gurg[i], 1.3), -6) for i in range(3)], [666969, 433840, 52204])
    voice("gorgeleech", "aggro", [fused(pitch(gurg[i], 1.25), suck, -9, shape="follow") for i in range(3)], [52204, 707321])
    voice("gorgeleech", "attack", [mix(yclick(i), level_to(pops[i], yclick(i), 2), 0.27) for i in range(3)], [175678, 444791, 386885])
    voice("gorgeleech", "hurt", [fused(head(pitch(gurg[3 + i % 2], 1.4), 0.7), slime[3 + i % 2], -8) for i in range(3)], [52204, 433840])
    voice("gorgeleech", "death", [tail_out(fused(pitch(gurg[0], 1.0), suck2, -6), 1.0),
                                  tail_out(mix(pitch(gurg[1], 0.9), level_to(grab(794272, 1, 0.2, 1.5)[0], gurg[1], -4), 0.3), 0.8)],
          [52204, 707322, 794272])
    voice("gorgeleech", "tell", [glide(mix(yclick(i), level_to(head(suck, 0.5), yclick(i), -10)), 0, 3) for i in range(2)], [175678, 707321])
    # the three-jaw Y-click, then the latch: a pop and the drawing suck
    voice("gorgeleech", "signature", [mix(mix(yclick(i), level_to(pops[4 + i], yclick(i), 3), 0.28), level_to(head([suck, suck2][i], 1.6), yclick(i), -2), 0.36)
                                      for i in range(2)], [175678, 444791, 386885, 707321, 707322])

    # ---- Hexcrow: raven croaks and knocks, rook caws, a human whisper reversed under the curse cry; feather rustle.
    raven = grab(275466, 3, 0.2, 1.5) + grab(519591, 3, 0.2, 1.5) + grab(572720, 2, 0.2, 1.5)
    rook = grab(96377, 3, 0.15, 1.2)
    caw = grab(741366, 4, 0.2, 1.2) + grab(386768, 3, 0.2, 1.2)
    whisper = [rev(w) for w in grab(193811, 2, 0.5, 3.0) + grab(193818, 1, 0.5, 3.0)]
    flut = grab(263483, 3, 0.2, 2.0)
    voice("hexcrow", "idle", [raven[0], raven[3], fused(raven[6], flut[0], -14)], [275466, 519591, 572720, 263483])
    voice("hexcrow", "aggro", [caw[0], rook[0], mix(caw[4], level_to(rook[1], caw[4], -3), 0.25)], [741366, 96377, 386768])
    voice("hexcrow", "attack", [mix(head(caw[i + 1], 0.6), level_to(flut[i], caw[i + 1], -4), 0.0) for i in range(3)], [741366, 263483])
    voice("hexcrow", "hurt", [pitch(head(c, 0.5), 1.12) for c in (rook[2], caw[5], raven[1])], [96377, 386768, 275466])
    voice("hexcrow", "death", [tail_out(fused(pitch(raven[4], 0.85), flut[1], -6), 0.8), tail_out(fused(pitch(caw[6], 0.82), flut[2], -6), 0.6)],
          [519591, 386768, 263483])
    voice("hexcrow", "tell", [fused(glide(caw[i], 0, 5), whisper[i], -9) for i in range(2)], [741366, 193811])
    # the rising curse caw, several crows in a ragged chorus, the reversed whisper under it
    mur = [chorus([caw[0], rook[0], caw[2], raven[2], caw[5]], [0, 0.11, 0.27, 0.38, 0.55], [1.0, 1.05, 0.95, 1.1, 0.92]),
           chorus([caw[1], caw[3], rook[2], raven[5], caw[6]], [0, 0.14, 0.22, 0.41, 0.6], [0.97, 1.08, 1.0, 0.9, 1.04])]
    voice("hexcrow", "signature", [fused(glide(m, 0, 4), whisper[(i + 1) % len(whisper)], -8) for i, m in enumerate(mur)],
          [741366, 96377, 275466, 519591, 386768, 193811, 193818])

    # ---- Bogstalker: horned frog scream, pig frog grunt, bullfrog; throat-sac pump, mud squelch, tongue crack.
    scream = grab(63705, 4, 0.2, 2.0)
    pig = grab(558488, 3, 0.15, 1.5) + grab(721978, 2, 0.3, 2.5)
    bull = grab(188723, 3, 0.3, 2.0, skip=2)
    squelch = grab(207142, 3, 0.1, 1.2) + grab(547446, 2, 0.1, 1.2)
    def pump(k):
        p = pitch(head(bull[k % 3], 0.35), 0.62)
        return mix(p, level_to(squelch[k % 5], p, -6))
    def tongue(k):
        c = pitch(clicks[k % 4], 0.8)
        return mix(c, level_to(slap[k % 3], c, -2), 0.02)
    voice("bogstalker", "idle", [fused(pig[i], squelch[i], -14) for i in range(3)], [558488, 207142])
    voice("bogstalker", "aggro", [mix(scream[i], level_to(pitch(bull[i], 0.8), scream[i], -5)) for i in range(3)], [63705, 188723])
    voice("bogstalker", "attack", [mix(tongue(i), level_to(squelch[3 + i % 2], tongue(i), -4), 0.1) for i in range(3)], [175678, 545593, 547446])
    voice("bogstalker", "hurt", [pitch(head(scream[3], 0.6), 1.1), head(pig[3], 0.6), pitch(head(scream[1], 0.5), 1.2)], [63705, 721978])
    voice("bogstalker", "death", [tail_out(pitch(pig[4], 0.8), 1.0), tail_out(mix(pitch(scream[2], 0.8), level_to(squelch[0], scream[2], -6), 0.4), 0.7)],
          [721978, 63705, 207142])
    voice("bogstalker", "tell", [seq([glide(pump(i), 0, 2), glide(pump(i + 1), 2, 4)], 0.42) for i in range(2)], [188723, 207142, 547446])
    # two throat pumps, then the wet tongue-crack
    voice("bogstalker", "signature", [seq([pump(i), pump(i + 1), tongue(i)], 0.4) for i in range(2)], [188723, 207142, 175678, 545593])

    # ---- Mirestrider: shoebill-like bill clatter (stork), heron squawk, egret, the low moo (hornbill duet pitched down); bill knock.
    clat = peaks(153500, 3, 1.6, band=(800, 7000)) + peaks(269223, 2, 1.8, band=(800, 7000))
    heron = grab(645157, 3, 0.2, 1.5)
    egret = grab(541377, 3, 0.2, 1.5)
    horn = grab(874260, 2, 0.4, 2.5)
    knock = grab(553886, 3, 0.05, 0.8)
    voice("mirestrider", "idle", [pitch(horn[0], 0.55), pitch(horn[1], 0.6), fused(egret[0], knock[0], -14)], [874260, 541377, 553886])
    voice("mirestrider", "aggro", [mix(heron[i], level_to(clat[i], heron[i], -3), 0.2) for i in range(3)], [645157, 153500])
    voice("mirestrider", "attack", [mix(knock[i], level_to(head(clat[3 + i % 2], 0.5), knock[i], -2), 0.03) for i in range(3)], [553886, 269223])
    voice("mirestrider", "hurt", [pitch(head(egret[1], 0.5), 1.1), pitch(head(heron[1], 0.5), 1.15), head(egret[2], 0.5)], [541377, 645157])
    voice("mirestrider", "death", [tail_out(pitch(heron[2], 0.82), 0.8), tail_out(pitch(horn[0], 0.5), 1.0)], [645157, 874260])
    voice("mirestrider", "tell", [glide(head(clat[i], 1.0), 0, 4) for i in range(2)], [153500])
    # the machine-gun clatter that turns a whole herd
    voice("mirestrider", "signature", [mix(mix(clat[0], level_to(clat[3], clat[0], -3), 0.35), level_to(knock[1], clat[0], 0), 0.0),
                                       mix(mix(clat[1], level_to(clat[4], clat[1], -3), 0.3), level_to(knock[2], clat[1], 0), 0.0)],
          [153500, 269223, 553886])

    # ---- Mudhulk: hippo wheeze-honk laugh, elephant rumble, walrus-like sea lion grunt; water churn.
    hippo = grab(876412, 3, 0.6, 4.0) + grab(103118, 2, 0.6, 4.0) + grab(194870, 2, 0.5, 4.0)
    ele = grab(810176, 3, 0.6, 4.0, band=(30, 1500))
    sealg = grab(849186, 3, 0.3, 2.0)
    churn = part(637974, 4.0)
    voice("mudhulk", "idle", [fused(hippo[5], churn, -14), fused(sealg[0], churn, -16), fused(pitch(ele[2], 0.9), churn, -16)],
          [194870, 849186, 810176, 637974])
    voice("mudhulk", "aggro", [mix(hippo[i], level_to(pitch(ele[i], 0.8), hippo[i], -5)) for i in range(3)], [876412, 810176])
    voice("mudhulk", "attack", [mix(head(sealg[i], 0.9), level_to(splash[i % 2], sealg[i], -2), 0.2) for i in range(3)], [849186, 442773])
    voice("mudhulk", "hurt", [head(hippo[3], 0.8), pitch(head(hippo[4], 0.7), 1.1), pitch(head(sealg[1], 0.6), 1.1)], [103118, 849186])
    voice("mudhulk", "death", [tail_out(fused(pitch(ele[0], 0.7), churn, -8), 1.5), tail_out(pitch(hippo[6], 0.8), 1.0)], [810176, 637974, 194870])
    yawn = [fused(mix(pitch(ele[i + 1], 0.78), level_to(pitch(sealg[2], 0.8), ele[i + 1], -4), 0.3), churn, -12) for i in range(2)]
    voice("mudhulk", "tell", [glide(y, -1, 2) for y in yawn], [810176, 849186, 637974])
    # the yawn groan before a charge
    voice("mudhulk", "signature", yawn, [810176, 849186, 637974])

    # ---- Bogsnout: babirusa-like grunts and teeth chatter, warthog squeal, peccary-like bark (boar); mud snuffle.
    grunts = grab(421734, 4, 0.15, 1.5) + grab(842313, 2, 0.2, 1.5)
    squeal = grab(344972, 3, 0.3, 2.0) + grab(147240, 2, 0.3, 2.0)
    sniff = grab(864329, 3, 0.2, 2.0)
    boar = grab(764944, 3, 0.4, 2.5)
    snuf = part(543775, 2.0)
    chatter = seq([pitch(clicks[k % 4], 1.6) for k in range(6)], 0.055)
    voice("bogsnout", "idle", [fused(sniff[i], snuf, -12) for i in range(2)] + [fused(grunts[0], snuf, -14)], [864329, 421734, 543775])
    voice("bogsnout", "aggro", [mix(boar[i], level_to(chatter, boar[i], -6), 0.05) for i in range(3)], [764944, 175678])
    voice("bogsnout", "attack", [fused(grunts[1 + i], snuf, -8) for i in range(2)] + [grunts[4]], [421734, 842313, 543775])
    voice("bogsnout", "hurt", [head(squeal[3], 0.6), head(squeal[4], 0.6), pitch(head(squeal[0], 0.5), 1.1)], [147240, 344972])
    voice("bogsnout", "death", [tail_out(pitch(squeal[1], 0.85), 0.8), tail_out(pitch(grunts[5], 0.8), 0.6)], [344972, 842313])
    voice("bogsnout", "tell", [glide(head(squeal[i], 0.9), 0, 5) for i in range(2)], [344972])
    # the rising squeal
    voice("bogsnout", "signature", [fused(glide(squeal[i], -2, 6), snuf, -14) for i in (2, 0)], [344972, 543775])

    # ---- Lanterntoad: cane-toad trill, common-toad chirp (and a toad trill); soft glass hum as the sac glows.
    trill = grab(412096, 3, 0.4, 3.0) + grab(469364, 3, 0.3, 2.5)
    chirp = grab(462863, 3, 0.1, 1.2) + grab(730711, 3, 0.1, 1.2)
    glass = part(338730, 4.0)
    voice("lanterntoad", "idle", [fused(trill[3 + i], glass, -14, shape="follow") for i in range(3)], [469364, 338730])
    voice("lanterntoad", "aggro", [fused(trill[i], glass, -10, shape="rise") for i in range(3)], [412096, 338730])
    voice("lanterntoad", "attack", chirp[3:6], [730711])
    voice("lanterntoad", "hurt", [pitch(head(c, 0.5), 1.15) for c in chirp[:3]], [462863])
    voice("lanterntoad", "death", [tail_out(fused(pitch(trill[0], 0.75), glass, -8), 0.8), tail_out(pitch(chirp[1], 0.7), 0.4)], [412096, 462863, 338730])
    voice("lanterntoad", "tell", [fused(glide(head(trill[i], 1.2), 0, 3), glass, -8, shape="rise") for i in range(2)], [412096, 338730])
    # the ring's trill swelling in turn round a knee
    ring = [chorus([trill[3], trill[0], trill[4], trill[1], trill[5]], [0, 0.35, 0.7, 1.05, 1.4], [1.0, 1.05, 0.95, 1.1, 0.98], [0, -3, -2, -1, 0]),
            chorus([trill[1], trill[5], trill[2], trill[3]], [0, 0.4, 0.8, 1.2], [1.02, 0.96, 1.08, 1.0], [0, -2, -1, 0])]
    voice("lanterntoad", "signature", [fused(ramp(r, 0.4, 1.0, 1.2), glass, -10, shape="rise") for r in ring], [412096, 469364, 338730])

    # ---- Reedfowl: purple swamphen honk, moorhen kek, coot; reed rustle.
    hen = grab(251671, 4, 0.2, 2.0)
    moor = grab(753805, 3, 0.1, 1.2) + grab(638277, 2, 0.2, 2.0)
    coot = grab(351896, 3, 0.1, 1.5)
    reed = part(411846, 2.5)
    voice("reedfowl", "idle", [fused(coot[i], reed, -12) for i in range(2)] + [moor[0]], [351896, 411846, 753805])
    voice("reedfowl", "aggro", [fused(hen[i], reed, -10) for i in range(3)], [251671, 411846])
    voice("reedfowl", "attack", [head(m, 0.5) for m in moor[1:3]] + [head(coot[2], 0.5)], [753805, 351896])
    voice("reedfowl", "hurt", [head(moor[3], 0.6), head(moor[4], 0.6), pitch(head(hen[3], 0.5), 1.15)], [638277, 251671])
    voice("reedfowl", "death", [tail_out(pitch(moor[3], 0.85), 0.6), tail_out(pitch(hen[2], 0.85), 0.6)], [638277, 251671])
    voice("reedfowl", "tell", [glide(head(hen[i], 0.8), 0, 3) for i in range(2)], [251671])
    # the alarm honk (and the reeds thrashing as it flushes)
    voice("reedfowl", "signature", [fused(seq([hen[0], pitch(hen[1], 1.05)], 0.45), reed, -6), fused(seq([hen[2], hen[3]], 0.4), reed, -6)],
          [251671, 411846])

    # ---- Slimeshell (no voice): a slow wet glide, a shell knock, the slime stretch, the withdraw clack.
    stretch = grab(569533, 3, 0.2, 2.5)
    crab = grab(770031, 4, 0.05, 1.0) + grab(761559, 2, 0.1, 1.5)
    snail = part(661853, 3.0)
    slaps = grab(794272, 3, 0.1, 1.2)
    voice("slimeshell", "idle", [fused(head(snail, 2.0), slime[i], -4) for i in range(2)] + [fused(crab[4], snail, -6)], [661853, 666969, 761559])
    voice("slimeshell", "aggro", [fused(stretch[i], crab[i], -6) for i in range(2)] + [stretch[2]], [569533, 770031])
    voice("slimeshell", "attack", [mix(slaps[i], level_to(crab[1 + i], slaps[i], 0), 0.05) for i in range(3)], [794272, 770031])
    voice("slimeshell", "hurt", [mix(crab[i], level_to(head(slime[i], 0.5), crab[i], -3), 0.02) for i in range(3)], [770031, 666969])
    voice("slimeshell", "death", [tail_out(mix(pitch(stretch[0], 0.75), level_to(crab[3], stretch[0], 0), 0.3), 0.8),
                                  tail_out(fused(pitch(slaps[2], 0.8), snail, -6), 0.6)], [569533, 770031, 794272, 661853])
    voice("slimeshell", "tell", [glide(head(stretch[i], 1.0), 0, 3) for i in range(2)], [569533])
    # the shell withdraw clack
    voice("slimeshell", "signature", [mix(seq([crab[0], pitch(crab[2], 0.9)], 0.07), level_to(pops[i], crab[0], -4), 0.12) for i in range(2)],
          [770031, 444791])

    # ---- Mother Maw: croc bellow deeper, a bittern-like boom (whale low and a bullfrog well down), alligator hiss; reed crack.
    whale = [tail_out(head(w, 3.0), 0.8) for w in grab(171945, 3, 1.0, 5.0, band=(30, 1200))]
    crack = grab(669457, 2, 0.1, 1.5) + grab(422623, 2, 0.1, 1.5)
    spray = grab(637974, 2, 0.3, 2.5)
    def boom(k):
        w = pitch(head(whale[k % 3], 1.4), 0.7)
        return mix(w, level_to(pitch(head(frog[k % 2], 0.5), 0.38), w, -2))
    def maw_bellow(k, p=0.62):
        b = pitch(bell[k % 3], p)
        return mix(b, level_to(boom(k), b, -5), 0.1)
    voice("mother_maw", "idle", [fused(boom(i), weed, -14) for i in range(3)], [171945, 188723, 737308])
    voice("mother_maw", "aggro", [maw_bellow(i) for i in range(3)], [272408, 171945, 188723])
    voice("mother_maw", "attack", [mix(pitch(head(hiss[i % 2], 0.8), 0.8), level_to(crack[i], hiss[i % 2], 0), 0.2) for i in range(4)],
          [675513, 669457, 422623])
    voice("mother_maw", "hurt", [pitch(head(c, 1.0), 0.75) for c in croc], [403373])
    voice("mother_maw", "death", [tail_out(mix(maw_bellow(0, 0.55), level_to(spray[0], bell[0], -4), 0.5), 2.0),
                                  tail_out(fused(pitch(growl[0], 0.6), weed, -8), 1.5)], [272408, 171945, 637974, 527844, 737308])
    voice("mother_maw", "tell", [fused(glide(pitch(hiss[i], 0.75), 0, 4), boom(i), -4) for i in range(2)], [675513, 171945])
    # the water dancing on her back as she bellows
    sigm = []
    for i in range(2):
        b = maw_bellow(i, 0.6)
        dance = tremble(bandpass(match(lap, len(b)), 300, 8000), 22 + 4 * i, 0.9)
        b = mix(b, level_to(dance, b, -3))
        sigm.append(mix(b, level_to(spray[i], b, -4), len(b) / SR * 0.4))
    voice("mother_maw", "signature", sigm, [272408, 171945, 188723, 827295, 637974])

    # ---- The Hex-Warden: a wordless female chant and breath, raven croak, laughter; bone chimes, a crow-wing burst.
    chant = grab(748552, 3, 1.0, 5.0)
    hum = grab(816688, 2, 1.0, 5.0)
    breath = grab(485402, 4, 0.3, 2.5)
    laugh = grab(179924, 3, 0.5, 3.0)
    chimes = grab(732525, 3, 0.3, 3.0)
    wingb = grab(572216, 2, 0.3, 2.0) + grab(596541, 2, 0.3, 2.0)
    voice("hex_warden", "idle", [fused(hum[i], chimes[i], -10) for i in range(2)] + [fused(pitch(chant[2], 0.9), chimes[2], -12)],
          [816688, 748552, 732525])
    voice("hex_warden", "aggro", [mix(laugh[i], level_to(wingb[i], laugh[i], -4), 0.3) for i in range(3)], [179924, 572216, 596541])
    voice("hex_warden", "attack", [mix(pitch(head(breath[i], 0.6), 1.1), level_to(head(wingb[i], 0.8), breath[i], -2), 0.05) for i in range(4)],
          [485402, 572216, 596541])
    voice("hex_warden", "hurt", [pitch(head(b, 0.6), 1.2) for b in breath[:3]], [485402])
    voice("hex_warden", "death", [tail_out(fused(pitch(chant[0], 0.8), grab(565911, 1, 1.0, 5.0)[0], -6), 2.0),
                                  tail_out(mix(pitch(laugh[2], 0.8), level_to(raven[2], laugh[2], -4), 0.4), 1.2)],
          [748552, 565911, 179924, 275466])
    voice("hex_warden", "tell", [fused(glide(head(chant[i], 1.4), 0, 3), chimes[i], -8, shape="rise") for i in range(2)], [748552, 732525])
    # the call that isn't hers: her chant thinning into a raven's croak over a reversed whisper (the mimic itself plays
    # the copied creature's own call from her mouth: creature.<family>.aggro at her head, pitched down)
    voice("hex_warden", "signature", [mix(fused(chant[i], whisper[i], -6), level_to(raven[1 + i * 3], chant[i], -2), len(chant[i]) / SR * 0.55)
                                      for i in range(2)], [748552, 193811, 275466, 519591])

    # ---- The Bloat: elephant-seal gurgle, walrus-like sea lion groan, a leech suck amplified; stretched-skin creak.
    groan = grab(184491, 3, 0.5, 3.0) + grab(849186, 2, 0.4, 2.5, skip=3)
    inflate = peaks(760345, 2, 3.0, pre=0.5) + peaks(501062, 1, 3.0, pre=0.9)
    voice("bloat", "idle", [fused(pitch(gurg[i], 0.7), stretch[i], -10) for i in range(3)], [52204, 569533])
    voice("bloat", "aggro", [fused(pitch(groan[i], 0.8), suck, -6, shape="follow") for i in range(3)], [184491, 707321])
    voice("bloat", "attack", [fused(pitch(head(groan[3 + i % 2], 1.0), 0.85), pitch(head(suck2, 1.0), 0.7), -2) for i in range(4)],
          [849186, 707322])
    voice("bloat", "hurt", [fused(head(pitch(gurg[i + 2], 0.85), 0.8), stretch[i], -6) for i in range(3)], [52204, 569533])
    voice("bloat", "death", [tail_out(fused(pitch(groan[0], 0.65), suck2, -4), 2.0), tail_out(pitch(gurg[4], 0.6), 1.5)], [184491, 707322, 52204])
    voice("bloat", "tell", [fused(glide(pitch(gurg[i], 0.8), 0, 5), stretch[i], -6) for i in range(2)], [52204, 569533])
    # the rising whine as it nears a burst: the skin stretching over the gurgle
    voice("bloat", "signature", [mix(fused(glide(inflate[i], 0, 4), pitch(gurg[i], 0.8), -4), level_to(stretch[i], inflate[i], -6), 0.3)
                                 for i in range(2)], [760345, 52204, 569533])

    # ---- Vharra: leopard-seal-like trill (sea lion scream, seal bark), bull-alligator bellow, whale groan pitched down;
    # a wet three-part jaw crack, a chain-like disc creak.
    trillv = grab(499564, 3, 0.4, 3.0) + grab(437960, 2, 0.2, 1.5)
    chain = grab(55034, 2, 0.3, 3.0) + grab(862897, 2, 0.3, 3.0)
    gore = [part(737308, 0.6, w) for w in range(3)]
    jaw = [mix(seq([pitch(crack[(k + j) % 4], [0.8, 0.9, 1.0][j]) for j in range(3)], 0.08), level_to(gore[k], crack[k], -4)) for k in range(3)]
    def vh_bellow(k, p=0.66):
        b = pitch(bell[k % 3], p)
        return mix(b, level_to(pitch(whale[k % 3], 0.5), b, -4), 0.05)
    voice("vharra", "idle", [fused(pitch(whale[i], 0.55), chain[i], -10) for i in range(3)], [171945, 55034, 862897])
    voice("vharra", "aggro", [mix(vh_bellow(i), level_to(pitch(trillv[i], 0.75), vh_bellow(i), -4), 0.4) for i in range(3)],
          [272408, 171945, 499564])
    voice("vharra", "attack", [mix(jaw[i % 3], level_to(pitch(head(hiss[i % 2], 0.6), 0.7), jaw[i % 3], -3)) for i in range(4)],
          [669457, 422623, 737308, 675513])
    voice("vharra", "hurt", [pitch(head(t, 0.9), 0.85) for t in trillv[:3]], [499564])
    voice("vharra", "death", [tail_out(echoes(mix(vh_bellow(0, 0.55), level_to(chain[3], bell[0], -6), 0.8)), 3.0),
                              tail_out(echoes(pitch(trillv[3], 0.6)), 2.0)], [272408, 171945, 862897, 437960])
    voice("vharra", "tell", [fused(glide(pitch(trillv[i], 0.8), 0, 4), chain[i], -8) for i in range(2)], [499564, 55034])
    # the trill echoing round the sinkhole before she rises
    voice("vharra", "signature", [echoes(mix(pitch(trillv[i], 0.78), level_to(pitch(trillv[3 + i], 0.7), trillv[i], -4), 0.5))
                                  for i in range(2)], [499564, 437960])
    json.dump(VOICE, open(os.path.join(WORK, "swamp_voices.json"), "w"))
    save_manifest()

# ================================================================ events (hazards, places, the bosses' moments, the Blood path)
EVENTS = {}
def event(key, clips, sources, folder="world"):
    """key 'swamp.plank.creak' -> world/swamp_plank_creak_1..n (folder 'spell' for the Blood path)."""
    files = []
    stem = key.replace(".", "_")
    for i, c in enumerate(clips):
        f = f"{folder}/{stem}_{i + 1}"
        write(f, c, "sfx", sources)
        files.append(f)
    EVENTS[key] = files

def bellcrack(b, p, detune=1.035):
    """A bell made old and cracked: the strike and a slightly sharp copy beating against it."""
    x = pitch(b, p)
    return tail_out(mix(x, level_to(pitch(b, p * detune), x, -5), 0.004), 0.3)

if want("events"):
    # -- hazards (cook 2.8): each tell >= 0.9 s, each answerable
    creak = grab(456814, 2, 0.4, 2.5) + grab(506665, 1, 0.3, 1.5) + grab(590171, 1, 0.4, 2.5)
    # the rotten plank's creak as weight lands: 0.9 s+, sagging (a little pitch fall) and wet (a squelch under it)
    sq = grab(207142, 2, 0.1, 1.2)
    event("swamp.plank.creak", [fused(glide(head(c, 1.3), 0, -1.5), sq[i % 2], -12) for i, c in enumerate(creak[:3])], [456814, 506665, 207142])
    brk = grab(518791, 1, 0.2, 2.0) + grab(520720, 1, 0.2, 2.0) + grab(669457, 1, 0.1, 1.5)
    spl = grab(320330, 2, 0.3, 2.5)
    event("swamp.plank.break", [mix(b, level_to(spl[i % 2], b, -3), 0.35) for i, b in enumerate(brk)], [518791, 520720, 669457, 320330])
    event("swamp.plank.repair", [mix(h, level_to(head(creak[3], 0.6), h, -6), 0.2) for h in grab(513346, 2, 0.1, 1.2) + grab(707864, 1, 0.1, 1.2)],
          [513346, 707864, 590171])
    # marsh gas: the rising whoomp 0.9 s before it lights, then the flash
    hiss_g = grab(212458, 2, 0.3, 3.0)
    whoosh = grab(260555, 2, 0.3, 2.5)
    bub = grab(352904, 2, 0.3, 3.0)
    event("swamp.gas.whoomp", [ramp(mix(head(bub[i], 1.1), level_to(glide(head(whoosh[i], 1.1), -3, 2), bub[i], 0), 0.0), 0.15, 1.0, 1.6)
                               for i in range(2)], [352904, 260555])
    event("swamp.gas.flash", [mix(c, level_to(head(hiss_g[i % 2], 1.2), c, -4), 0.02) for i, c in enumerate(grab(412558, 2, 0.3, 3.0) + grab(457209, 1, 0.3, 3.0))],
          [412558, 457209, 212458])
    # bleedwater: the soft wet suck at the edge (the 0.9 s ripple toward your foot), and the leeches' hum
    s1, s2 = part(707322, 1.4), part(269339, 1.4)
    event("swamp.bleed.suck", [ramp(fused(s1, part(539823, 1.4), -8), 0.2, 1.0, 1.4), ramp(fused(s2, part(207142, 1.4), -6), 0.2, 1.0, 1.4),
                               ramp(fused(pitch(s1, 0.85), part(547446, 1.6), -6), 0.2, 1.0, 1.4)], [707322, 269339, 539823, 207142, 547446])
    drone = part(567148, 3.0, band=(40, 400))
    hum = part(338730, 3.0)
    event("swamp.leech.hum", [ramp(mix(head(drone, 1.6), level_to(head(pitch(hum, 0.7 + 0.08 * i), 1.6), drone, -6)), 0.1, 1.0, 1.5) for i in range(2)],
          [567148, 338730])
    # quag: the sucking mud on the first step and as it holds you
    event("swamp.quag.suck", [fused(m, part(707321, 1.5), -10) for m in grab(543775, 2, 0.3, 2.0) + grab(547446, 1, 0.3, 2.0, skip=2)],
          [543775, 547446, 707321])
    # a stormglass rod / a strike shocking the water
    zap = grab(160421, 1, 0.2, 2.0) + grab(853286, 1, 0.2, 1.5) + peaks(398274, 1, 1.4)
    sizz = part(552723, 1.5)
    event("swamp.rod.shock", [fused(z, sizz, -6) for z in zap], [160421, 853286, 398274, 552723])
    event("swamp.water_shock", [mix(fused(z, sizz, -3), level_to(grab(545593, 1, 0.1, 1.0)[0], z, -4), 0.05) for z in zap[:2]],
          [160421, 853286, 552723, 545593])
    # the shrines' wind chimes and bone chimes, the cracked bell
    event("swamp.chimes", grab(437337, 1, 1.0, 6.0) + grab(398496, 1, 0.5, 4.0) + grab(732525, 1, 0.5, 4.0), [437337, 398496, 732525])
    toll = peaks(454855, 1, 5.0, pre=0.05)[0]
    church = peaks(518343, 1, 5.0, pre=0.05)[0]
    low = peaks(554655, 1, 4.0, pre=0.05)[0]
    event("swamp.bell", [bellcrack(toll, 0.8, 1.03), bellcrack(church, 0.75, 1.04), bellcrack(low, 0.9, 1.025)], [454855, 518343, 554655])
    # the Red Tide's warning: a deep gong under a drone, every Hexcrow murder lifting off (2D, heard across the swamp)
    gong = peaks(415200, 2, 7.0, pre=0.05)
    flock = grab(565911, 1, 1.0, 6.0) + grab(214869, 1, 1.0, 6.0)
    deep = part(632682, 7.0, band=(30, 300))
    event("swamp.tide.warning", [mix(mix(pitch(g, 0.7), level_to(deep, g, -6)), level_to(bandpass(flock[i], 300, 6000), g, -8), 1.5) for i, g in enumerate(gong)],
          [415200, 632682, 565911, 214869])
    event("swamp.crows", [fused(f, grab(689998, 1, 0.3, 3.0)[0], -4) for f in flock], [565911, 214869, 689998])
    # -- places and things (WRSwampSystems)
    event("swamp.candle.out", grab(242867, 1, 0.1, 1.5) + grab(406647, 1, 0.1, 1.5) + grab(656818, 1, 0.1, 1.5), [242867, 406647, 656818])
    event("swamp.key", [mix(c, level_to(head(peaks(625616, 1, 1.0)[0], 0.8), c, -4), 0.1) for c in grab(862897, 2, 0.3, 2.0)], [862897, 625616])
    event("swamp.trapdoor", [mix(head(creak[i], 0.9), level_to(grab(553886, 2, 0.05, 0.8)[i], creak[i], 2), 0.6) for i in range(2)],
          [456814, 553886])
    event("swamp.ladder", [mix(head(creak[3], 0.7), level_to(head(c, 0.6), creak[3], -2), 0.3) for c in creak[1:3]], [590171, 456814, 506665])
    event("swamp.gate", [fused(head(peaks(625616, 2, 2.5)[i], 2.2), head(creak[i], 2.2), -6) for i in range(2)], [625616, 456814])
    event("swamp.dive", [mix(head(s, 1.6), level_to(part(454338, 1.6, band=(60, 1200)), s, -6), 0.3) for s in spl], [320330, 454338])
    event("swamp.egg.tear", [fused(head(g, 1.0), head(stretch_e, 1.0), -4) for g, stretch_e in
                             zip(grab(737308, 2, 0.2, 1.5), grab(569533, 2, 0.2, 2.0))], [737308, 569533])
    jars = grab(444448, 1, 0.2, 2.0) + grab(447699, 1, 0.2, 2.0) + grab(212698, 1, 0.2, 2.0)
    event("swamp.jar.burst", [mix(j, level_to(fused(zap[i % 2], sizz, -6), j, -6), 0.02) for i, j in enumerate(jars)], [444448, 447699, 212698, 160421, 853286])
    event("swamp.salt.burst", [mix(head(peaks(432350, 1, 1.2)[0], 1.0), level_to(part(sid, 1.2), j, -2), 0.05) for j, sid in zip(jars[:2], (478800, 569609))],
          [432350, 478800, 569609])
    # -- the bosses' moments
    burst_pop = peaks(501062, 1, 1.0, pre=0.85)[0]
    gush = grab(345985, 1, 0.3, 2.5) + grab(654101, 1, 0.3, 2.5) + grab(563315, 1, 0.3, 2.5)
    event("swamp.bloat.burst", [mix(mix(pitch(burst_pop, 0.55), level_to(g, burst_pop, 0), 0.03), level_to(spl[i % 2], g, -4), 0.2) for i, g in enumerate(gush)],
          [501062, 345985, 654101, 563315, 320330])
    event("swamp.bloat.spew", [pitch(v, 0.7) for v in grab(338113, 1, 0.5, 3.0) + grab(370710, 1, 0.5, 3.0) + grab(232540, 1, 0.5, 3.0)],
          [338113, 370710, 232540])
    rush = part(439232, 5.0)
    event("swamp.vharra.flood", [ramp(mix(rush, level_to(part(454338, 5.0), rush, -3)), 0.2, 1.0, 0.8)], [439232, 454338])
    event("swamp.vharra.rise", [mix(pitch(s, 0.6), level_to(part(637974, 2.0, w), s, -2), 0.1) for w, s in enumerate(grab(442773, 2, 0.3, 3.0))],
          [442773, 637974])
    event("swamp.vharra.creak", [pitch(c, 0.7) for c in grab(55034, 2, 0.3, 3.0)], [55034])
    event("swamp.hex.wings", [pitch(w, 0.85) for w in grab(572216, 1, 0.3, 2.0) + grab(596541, 1, 0.3, 2.0)], [572216, 596541])
    json.dump(EVENTS, open(os.path.join(WORK, "swamp_events.json"), "w"))
    save_manifest()

# ================================================================ the Blood & curse path (cook 2.17): spell/blood_*
if want("blood"):
    hb = grab(249716, 4, 0.3, 1.5)            # a slow heartbeat's beats
    hbf = grab(688735, 4, 0.15, 1.0)          # a quicker one
    gush = grab(345985, 2, 0.3, 2.5) + grab(654101, 1, 0.3, 2.5) + grab(563315, 1, 0.3, 2.5)
    squirt = grab(453769, 3, 0.1, 1.5) + grab(800865, 2, 0.1, 1.5)
    stab = grab(399616, 2, 0.1, 1.0)
    slash = grab(441666, 1, 0.1, 1.0) + grab(574820, 1, 0.1, 1.0) + grab(842363, 1, 0.1, 1.5)
    whis = grab(193811, 2, 0.5, 3.0) + grab(193818, 1, 0.5, 3.0)
    brew = part(614866, 3.0)
    boil = part(343744, 3.0); boil2 = part(451287, 3.0)
    bubs = grab(539823, 3, 0.05, 1.0)
    choir = part(819349, 4.0)
    cello = part(867608, 4.0)
    drone = part(632682, 4.0, band=(30, 400))
    straw = part(269339, 1.5)
    gulp = grab(403935, 1, 0.1, 1.2) + grab(531755, 1, 0.1, 1.2) + grab(761619, 1, 0.1, 1.2)
    def beat(k, p=1.0): return pitch(hb[k % 4], p)
    B = lambda key, clips, src: event("blood." + key, clips, src, folder="spell")
    # the cast: a heartbeat thud under a wet draw (the blood leaving the hand)
    B("cast", [mix(beat(i), level_to(head(glide(part(707322, 0.8, i % 2), 0, 3), 0.7), beat(i), -2), 0.05) for i in range(3)], [249716, 707322])
    B("draw", [ramp(fused(head(part(707322, 1.2, i), 1.0), beat(i, 0.9), -6), 0.3, 1.0, 1.2) for i in range(2)], [707322, 249716])
    B("bolt", [mix(squirt[i], level_to(head(slash[i % 3], 0.4), squirt[i], -6), 0.0) for i in range(3)], [453769, 800865, 441666, 574820, 842363])
    B("hit", [mix(gush[i], level_to(squirt[3 + i % 2], gush[i], -3)) for i in range(3)], [345985, 654101, 563315, 800865])
    B("spear", [mix(stab[i % 2], level_to(squirt[i], stab[i % 2], -2), 0.03) for i in range(3)], [399616, 453769])
    B("boil", [fused(head(boil, 2.5), head(boil2, 2.5), -3), fused(head(boil2, 2.5), head(boil, 2.5), -6)], [343744, 451287])
    B("boil_pop", [mix(b, level_to(squirt[i], b, -6), 0.02) for i, b in enumerate(bubs)], [539823, 453769, 800865])
    B("tide", [ramp(mix(part(454338, 2.5), level_to(gush[i], part(454338, 2.5), -3), 0.4), 0.3, 1.0, 0.6) for i in range(2)], [454338, 345985, 654101])
    B("hex", [mix(rev(w), level_to(head(brew, len(w) / SR), w, -6)) for w in whis], [193811, 193818, 614866])
    B("misfortune_whiff", [mix(head(slash[i], 0.5), level_to(head(rev(whis[i]), 0.6), slash[i], -6)) for i in range(2)], [441666, 574820, 193811])
    toll = peaks(454855, 1, 5.0, pre=0.05)[0]
    gong = peaks(415200, 1, 6.0, pre=0.05)[0]
    B("doom", [mix(pitch(gong, 0.6), level_to(head(drone, 4.0), gong, -6)), bellcrack(toll, 0.55, 1.04)], [415200, 632682, 454855])
    B("doom_toll", [tail_out(head(bellcrack(toll, 0.6, 1.04), 4.5), 2.5), tail_out(head(bellcrack(toll, 0.66, 1.03), 4.5), 2.5)], [454855])
    B("doom_crack", [mix(c, level_to(g, c, -3), 0.02) for c, g in zip(grab(422623, 2, 0.1, 1.5), grab(212698, 1, 0.1, 1.5) + grab(540451, 1, 0.1, 1.5))],
      [422623, 212698, 540451])
    B("lifesteal", [mix(head(straw, 1.0), level_to(g, straw, -2), 0.5) for g in gulp], [269339, 403935, 531755, 761619])
    B("transfusion", [mix(head(part(432350, 1.6, i), 1.4), level_to(beat(i + 1), part(432350, 1.6, i), -2), 0.6) for i in range(2)], [432350, 249716])
    B("feast_heartbeat", [seq([beat(0), beat(1, 1.03), beat(2, 1.06)], 0.62)], [249716])
    B("heartbeat", [seq([beat(0), beat(1)], 0.94), seq([beat(2), beat(3)], 0.94)], [249716])
    # the Spilled bar's rally: the heart quickening, a choir swelling under it
    B("rally", [mix(seq([pitch(hbf[k % 4], 0.95) for k in range(6)], 0.4), level_to(ramp(head(choir, 2.6), 0, 1, 1.5), hbf[0], -2)) for _ in range(1)],
      [688735, 819349])
    B("frenzy", [mix(seq([hbf[k % 4] for k in range(8)], 0.28), level_to(head(grab(764944, 1, 0.4, 2.5)[0], 2.0), hbf[0], -6), 0.2)], [688735, 764944])
    B("lord", [mix(mix(head(choir, 4.0), level_to(head(drone, 4.0), choir, -4)), level_to(seq([beat(0, 0.8), beat(1, 0.8)], 1.1), choir, 0), 0.4)],
      [819349, 632682, 249716])
    B("proc", [mix(gush[i], level_to(beat(i, 0.85), gush[i], -2)) for i in range(3)], [345985, 654101, 563315, 249716])
    B("iceveins", [mix(head(part(552723, 1.5), 1.2), level_to(head(grab(450871, 1, 0.2, 2.0)[0], 1.2), part(552723, 1.5), -2), 0.05)],
      [552723, 450871])
    B("contagion_hiss", [fused(head(part(569609, 1.2, i), 1.0), head(rev(whis[i]), 1.0), -6) for i in range(2)], [569609, 193811])
    B("ruin_hum", [ramp(mix(head(cello, 3.0), level_to(head(drone, 3.0), cello, -4)), 0.2, 1.0, 1.0)], [867608, 632682])
    B("leech", [mix(head(slash[i], 0.5), level_to(head(part(707321, 0.8, i), 0.7), slash[i], -2), 0.1) for i in range(2)], [441666, 574820, 707321])
    B("slash", [mix(slash[i], level_to(squirt[i], slash[i], -5), 0.05) for i in range(3)], [441666, 574820, 842363, 453769])
    json.dump(EVENTS, open(os.path.join(WORK, "swamp_blood.json"), "w"))
    save_manifest()

# ================================================================ ambient beds (~60 s loops; cook 3.4)
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

BEDS = {}
def bed(name, x, sources):
    write(f"amb/swamp_{name}", loop_xfade(x, 4.0), "bed", sources, q=1)
    BEDS[name] = f"amb/swamp_{name}"

if want("beds"):
    swampy = bed_src(547690)
    lapb = bed_src(827295)
    # the swamp's own air under every sub-zone: a low wet wash and the water lapping
    bed("air", mix(bandpass(swampy, hi=2500), level_to(bandpass(lapb, hi=3000), swampy, -6)), [547690, 827295])
    wet = bed_src(854292)
    wrens = bed_src(847253)
    bed("day", mix(wet, level_to(bandpass(wrens, 400, 9000), wet, -3)), [854292, 847253])
    ins = bed_src(574670); frogs = bed_src(828795)
    bed("night", mix(ins, level_to(frogs, ins, -4)), [574670, 828795])
    # M1 Reed Flats: wind through reeds, coots and moorhens out on the flats; at night the frog chorus
    reeds = bed_src(844604); reeds2 = bed_src(411846)
    fowl = grab(351896, 3, 0.1, 1.5) + grab(753805, 2, 0.1, 1.2)
    bed("reeds", sprinkle(mix(reeds, level_to(reeds2, reeds, -4)), [bandpass(f, hi=6000) for f in fowl], 9, -10, 1), [844604, 411846, 351896, 753805])
    bed("reeds_night", mix(bed_src(569543), level_to(bed_src(546238), bed_src(569543), -5)), [569543, 546238])
    # M2 Mangroves: mangrove roots at the tide's edge, snails and crabs ticking, a hornbill far off
    mang = bed_src(733186); snails = bed_src(661853)
    bed("mangroves", sprinkle(mix(mang, level_to(snails, mang, -6)), [pitch(c, 0.9) for c in grab(874260, 2, 0.4, 2.5)], 18, -14, 2),
        [733186, 661853, 874260])
    # M3 Boardwalks: murk lapping under the planks, the planks settling, a raven's knock
    creaks = grab(456814, 2, 0.4, 2.5) + grab(506665, 1, 0.3, 1.5) + grab(590171, 1, 0.4, 2.5)
    bed("boardwalks", sprinkle(sprinkle(mix(lapb, level_to(bandpass(swampy, hi=2000), lapb, -4)), creaks, 7, -8, 3),
                               grab(572720, 2, 0.2, 1.5), 20, -14, 4), [827295, 547690, 456814, 506665, 590171, 572720])
    # M4 Murk Pools: the swamp's hum, fat bubbles rising out of the murk; at night its frogs and insects
    pools = bed_src(479575)
    bubbles = grab(352904, 3, 0.3, 3.0) + grab(539823, 2, 0.05, 1.0)
    bed("pools", sprinkle(pools, bubbles, 5, -8, 5), [479575, 352904, 539823])
    bed("pools_night", mix(bed_src(137961), level_to(ins, bed_src(137961), -8)), [137961, 574670])
    # M5 Bone Mounds: a low drone in the ground, bone chimes knocking in the wind, crows
    low = bandpass(bed_src(567148), 30, 600)
    wind = bed_src(844604, start=5.0)
    bones = grab(732525, 3, 0.3, 3.0)
    crows = grab(741366, 2, 0.2, 1.2) + grab(386768, 2, 0.2, 1.2)
    bed("bones", sprinkle(sprinkle(mix(bandpass(wind, hi=1500), level_to(low, wind, -4)), bones, 8, -6, 6), crows, 14, -12, 7),
        [567148, 844604, 732525, 741366, 386768])
    # the Weeping Wall's falls: the roar rising toward the cliffs (the director fades it by distance)
    fall = bed_src(650699); fall2 = bed_src(806569)
    bed("falls", mix(fall, level_to(bandpass(fall2, hi=1500), fall, -4)), [650699, 806569])
    # bleedwater round you: the leeches' low suck and hum in the water
    bw = sprinkle(mix(bandpass(bed_src(343744), hi=1200), level_to(bandpass(bed_src(567148), 30, 300), bed_src(343744), -6)),
                  [head(part(707322, 2.0, k), 1.6) for k in range(2)], 4, -8, 8)
    bed("bleedwater", bw, [343744, 567148, 707322])
    # the Red Tide: the boil in the turned pools
    bed("red_tide", sprinkle(mix(bed_src(451287), level_to(bed_src(352904), bed_src(451287), -2)), bubbles, 2.5, -4, 9), [451287, 352904, 539823])
    # the Sunken Coven: drips in stone, a muffled heartbeat (as if underwater), a whisper far off
    drips = bed_src(177958); drips2 = bed_src(850743)
    heart = match(stereo(bandpass(seq([pitch(c, 0.9) for c in grab(249716, 4, 0.3, 1.5)] * 2, 0.94), hi=250)), len(drips))
    whisp = [bandpass(w, hi=2000) for w in grab(193811, 2, 0.5, 3.0) + grab(193818, 1, 0.5, 3.0)]
    bed("coven", sprinkle(mix(mix(drips, level_to(drips2, drips, -4)), level_to(heart, drips, -6)), whisp, 12, -14, 10), [177958, 850743, 249716, 193811, 193818])
    json.dump(BEDS, open(os.path.join(WORK, "swamp_beds.json"), "w"))
    save_manifest()

