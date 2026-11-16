# The audio cook pass (cooks/cookpass-audio.md in showcase-notes): builds into Data/Audio the Fire / Frost and borrowed-pool
# lands' music (Kevin MacLeod, real instruments only), the combat entry and boss-fall stingers, the ten Fire / Frost
# sub-zone beds, the sixteen ff.* hazard sounds, the spot sounds, the sleep breaths and the families' alarm / bolt calls
# (Freesound CC0 / CC-BY recordings, sources_cookpass.py). Same rules as the other passes: short sounds by their loudest
# 400 ms (-16 LUFS), beds by integrated loudness (-23, seamless loops), music by integrated loudness (-18; calm -21, boss
# -16, layers -24), true-ish peak under -1 dBFS. ffmpeg decodes and encodes (Vorbis q1.5 music, q1 beds, q4 effects).
# Usage: python build_cookpass_audio.py <work> <Data/Audio> [music|stingers|beds|events|spots|voices ...] [force]
# Needs numpy, scipy, soundfile and ffmpeg (FFMPEG env var or on PATH). Writes <work>/cookpass_manifest.json (credits)
# and <work>/cookpass_events.json (the event -> files lists the table rows use).
import json, os, sys, math, subprocess, tempfile
import numpy as np, soundfile as sf
from scipy.signal import lfilter, resample_poly, butter, sosfilt
from scipy.ndimage import minimum_filter1d
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sources_cookpass import MUSIC, FREESOUND

WORK, OUT = sys.argv[1], sys.argv[2]
ONLY = set(sys.argv[3:])
FFMPEG = os.environ.get("FFMPEG", "ffmpeg")
SR = 44100
rng = np.random.default_rng(54)

def want(part):
    parts = ONLY - {"force"}
    return not parts or part in parts

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
mpath = os.path.join(WORK, "cookpass_manifest.json")
if os.path.exists(mpath): manifest = json.load(open(mpath))
EVENTS = {}
epath = os.path.join(WORK, "cookpass_events.json")
if os.path.exists(epath): EVENTS = json.load(open(epath))

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

def encode(path, x, q, ch):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        wav = os.path.join(td, "a.wav")
        sf.write(wav, np.clip(x, -1, 1).astype(np.float32), SR, subtype="FLOAT")
        subprocess.run([FFMPEG, "-v", "error", "-y", "-i", wav, "-c:a", "libvorbis", "-q:a", str(q), "-ac", str(ch), path], check=True)

def write(name, x, kind, sources, q=None, target=None):
    """kind: sfx (mono, loudest 400 ms at -16 LUFS), sting (stereo, the same), bed (stereo loop, -23 integrated),
    music (stereo, `target` integrated: -18, calm -21, boss -16, layers -24)."""
    path = os.path.join(OUT, name + ".ogg")
    if kind == "sfx":
        x = mono(fade(normalize_short(x, target or -16.0))); q = q or 4; ch = 1
    elif kind == "sting":
        x = stereo(fade(normalize_short(x, target or -16.0), 0.004, 0.3)); q = q or 4; ch = 2
    elif kind == "bed":
        x = stereo(normalize_long(x, -23.0)); q = q or 1; ch = 2
    else:
        x = stereo(fade(normalize_long(x, target or -18.0), 0.01, 0.05)); q = q or 1.5; ch = 2
    encode(path, x, q, ch)
    manifest[name] = {"sources": sorted(set(str(s) for s in sources)), "seconds": round(len(x) / SR, 2), "bytes": os.path.getsize(path)}
    print(f"{name:40} {len(x) / SR:7.2f}s {os.path.getsize(path) // 1024:6} KB", flush=True)

def save():
    json.dump(manifest, open(mpath, "w"), indent=1)
    json.dump(EVENTS, open(epath, "w"), indent=1)

def event(name, clips, sources, kind="sfx", target=None):
    """Writes clips as <name>_<n> and records the event's file list."""
    files = []
    for i, c in enumerate(clips):
        f = f"{name}_{i + 1}"
        write(f, c, kind, sources, target=target)
        files.append(f)
    EVENTS[name] = files

def data(rel):
    return load(os.path.join(OUT, rel + ".ogg"))

# ================================================================ music (cooks/cookpass-audio.md 4.2)
# Loudness by role (FULL-GAME 3.3): explore / mini / combat -18, calm cues (night, camp, caves, sub-zones) -21, boss -16,
# event layers -24 (they lie under whatever plays). Explore pieces end on their own; long ones are cut at 240 s with an
# 8 s fade; layers loop a 2-minute stretch from the middle with a 4 s crossfade.
CALM = ("night", "camp", "kiln_roads", "blue_below")
if want("music"):
    for name, (title, isrc, bpm, cue) in MUSIC.items():
        if os.path.exists(os.path.join(OUT, "music", name + ".ogg")) and "force" not in ONLY:
            continue
        x = trim(load(music_src(title)), -60)
        layer = name in ("vol_eruption", "tun_whiteout")
        if layer:
            mid = max(0.0, (len(x) / SR - 124) / 2)
            x = loop_xfade(cut(x, mid, mid + 124), 4.0)
            target = -24.0
        else:
            if len(x) > 240 * SR:
                x = cut(x, 0, 240)
                n = int(8 * SR); x[-n:] *= np.linspace(1, 0, n)[:, None] ** 1.5
            target = -16.0 if "_boss_" in name else -21.0 if any(c in name for c in CALM) else -18.0
        write("music/" + name, x, "music", ["km:" + title], target=target)
    save()

# ================================================================ stingers (music category, 2D)
if want("stingers"):
    # The entry hits that hide the change into a fight (RDR2): a taiko for the Volcanic, a frame drum for the Tundra,
    # a timpani everywhere else.
    event("music/sting_combat_volcanic", [fade(trim(load(fs(486141)), -50), 0.002, 0.25), fade(trim(load(fs(486140)), -50), 0.002, 0.25)], [486141, 486140], "sting", -14.0)
    event("music/sting_combat_tundra", [fade(trim(load(fs(345636)), -50), 0.002, 0.3)], [345636], "sting", -14.0)
    event("music/sting_combat", [fade(trim(load(fs(573282)), -50), 0.002, 0.4)], [573282], "sting", -14.0)
    # A boss's fall: the last held chord of Death of Kings (brass remembrance), 9 s, eased in.
    dk = trim(load(music_src("Death of Kings")), -60)
    fall = cut(dk, len(dk) / SR - 9.5, len(dk) / SR)
    n = int(0.35 * SR); fall[:n] *= np.linspace(0, 1, n)[:, None]
    event("music/sting_boss_fall", [fall], ["km:Death of Kings"], "sting", -16.0)
    save()

# ================================================================ Fire and Frost beds (cookpass-world 3.2; ~60-80 s loops)
def bed_src(sid, secs=64, start=None):
    x = load(fs(sid))
    if start is None: start = max(0.0, (len(x) / SR - secs) / 2)
    return match(cut(x, start, start + secs), int(secs * SR))

def bed(name, x, sources):
    write(f"amb/ff_{name}", loop_xfade(x, 3.0), "bed", sources)

if want("beds"):
    bed("v1", bed_src(697217, 40, 0.5), [697217])                                     # wind over the open plain
    bed("v2", bandpass(pitch(bed_src(415608, 72), 0.85), hi=5000), [415608])          # mud volcanoes slowed: lava gurgle
    bed("v3", bed_src(779944, 72), [779944])                                          # wind whistling on edges
    fum = bed_src(262671, 58, 1.5)
    bed("v4", mix(fum, level_to(bed_src(415609, 58), fum, -6)), [262671, 415609])     # fumarole hiss over bubbling mud
    bed("v5", bandpass(bed_src(663040, 90), hi=3000), [663040])                       # Yasur from the bottom: the crater roar
    bed("t1", bed_src(454372, 80), [454372])                                          # winter wind blowing powder snow
    bed("t2", bed_src(563571, 80), [563571])                                          # wind in a forest, a creaking trunk
    ice = bed_src(613571, 80)
    bed("t3", mix(ice, level_to(bed_src(847807, 80), ice, -6)), [613571, 847807])     # lake ice freezing, pinging, rumbling
    bed("t4", bed_src(615896, 70, 2.0), [615896])                                     # glacier melt: groans and meltwater
    bed("t5", bed_src(185070, 60, 1.0), [185070])                                     # howling wind on the plateau
    save()

# ================================================================ Fire and Frost hazards (cookpass-world 3.3-3.4)
def t(sid, db=-50): return fade(trim(load(fs(sid)), db), 0.004, 0.08)

if want("events"):
    hiss = t(234782)
    event("world/ff_fumarole_tell", [ramp(hiss, 0.25, 1.0, 1.5), ramp(peaks(262671, 1, 1.2, pre=0.1)[0], 0.25, 1.0, 1.5)], [234782, 262671])
    event("world/ff_fumarole", [t(90143), peaks(18405, 1, 3.0, pre=0.1)[0]], [90143, 18405])
    event("world/ff_geyser_tell", peaks(456806, 2, 1.2, pre=0.2), [456806])
    event("world/ff_geyser", peaks(263879, 2, 4.0, pre=0.15, gap=12), [263879])
    cracks = peaks(737896, 2, 1.4, pre=0.1)
    pour = peaks(162674, 2, 1.4, pre=0.1)
    event("world/ff_serac_tell", [mix(c, level_to(pour[i], c, -6), 0.2)[: len(c) + int(0.6 * SR)] for i, c in enumerate(cracks)], [737896, 162674])
    hitav = t(457528)
    event("world/ff_serac_fall", [mix(c, level_to(hitav, c, -4)) for c in peaks(271008, 2, 4.0, pre=0.15, gap=10)], [271008, 457528])
    event("world/ff_ice_crack", [t(262635), t(665084), t(764657)], [262635, 665084, 764657])
    event("world/ff_ice_creak", peaks(66930, 2, 1.2, pre=0.2), [66930])
    splash = mono(data("foot/water_2"))
    event("world/ff_ice_break", [mix(t(s), level_to(splash, t(s), -3), 0.15) for s in (666482, 422669)], [666482, 422669])
    event("world/ff_ice_refreeze", peaks(613571, 1, 2.0, pre=0.3), [613571])
    crunch = peaks(173190, 2, 1.0, pre=0.2)
    trickle = cut(load(fs(162674)), 0, 1.0)
    event("world/ff_bridge_creak", [mix(c, level_to(trickle, c, -5), 0.3) for c in crunch], [173190, 162674])
    event("world/ff_bridge_fall", [t(162674), peaks(426174, 1, 4.0, pre=0.1)[0]], [162674, 426174])
    event("world/ff_bridge_reform", peaks(452094, 1, 2.5, pre=0.3), [452094])
    event("world/ff_gust_howl", [ramp(t(738085), 0.3, 1.0, 1.2), ramp(t(379472), 0.3, 1.0, 1.2)], [738085, 379472])
    event("world/ff_bellows", [pitch(t(260555), 0.7), pitch(t(244926), 0.7)], [260555, 244926])
    event("world/ff_cup_light", [t(386385)], [386385])
    save()

# ================================================================ spot sounds (cookpass-audio 4.6)
if want("spots"):
    event("amb/ff_crater_chuff", [bandpass(c, hi=3000) for c in peaks(663040, 3, 6.0, pre=0.15, gap=25)], [663040])
    event("amb/ff_rockfall", [bandpass(t(s), hi=6000) for s in (426318, 567249, 389618)], [426318, 567249, 389618])
    event("amb/ff_crust_crack", [t(348942), pitch(peaks(764657, 1, 1.2, pre=0.1)[0], 1.3), pitch(t(262635), 1.5)], [348942, 764657, 262635])
    event("amb/ff_mudpot", grab(456806, 3, 0.08, 0.9, length=0.8), [456806])
    event("amb/ff_raven", grab(346318, 2, 0.2, 1.5) + [t(716962)], [346318, 716962])
    event("amb/ff_lake_ice", [bandpass(c, 60, 9000) for c in peaks(737896, 3, 3.0, pre=0.1, gap=4, skip=2)], [737896])
    event("amb/ff_frost_crack", [pitch(t(452109), 0.85), pitch(t(452109), 0.75)], [452109])
    event("amb/ff_ice_groan", [c for c in peaks(615896, 2, 4.0, band=(40, 600), pre=0.3, gap=8)] + peaks(103116, 1, 4.0, band=(40, 600), pre=0.3), [615896, 103116])
    event("amb/ff_howl_far", [bandpass(peaks(378334, 1, 7.0, pre=0.05)[0], hi=2500), bandpass(t(472402), hi=2500)], [378334, 472402])
    save()

# ================================================================ voices: sleep breaths and family alarm / bolt calls (cookpass-audio 4.8)
if want("voices"):
    event("creature/sleep_small", grab(561004, 3, 0.5, 3.0, length=1.8), [561004])
    event("creature/sleep_medium", grab(709430, 3, 0.8, 4.0, length=2.6), [709430])
    event("creature/sleep_large", grab(832102, 2, 0.8, 3.5, length=3.0) + grab(275388, 1, 0.8, 3.5, length=3.0), [832102, 275388])
    event("creature/canine_alert", [t(630648), t(277058)], [630648, 277058])
    event("creature/canine_flee", peaks(827539, 2, 0.9, pre=0.1), [827539])
    event("creature/herd_alert", [t(475483)] + grab(437111, 2, 0.3, 1.5, length=1.0), [475483, 437111])
    event("creature/herd_flee", [t(669872), t(677218)], [669872, 677218])
    event("creature/flyer_alert", peaks(812026, 2, 0.9, pre=0.1) + [t(316920)], [812026, 316920])
    event("creature/flyer_flee", peaks(164477, 2, 1.5, pre=0.1), [164477])
    event("creature/critter_alert", grab(640992, 2, 0.08, 1.0, length=0.6), [640992])
    event("creature/critter_flee", [t(288941), peaks(435748, 1, 0.9, pre=0.1)[0]], [288941, 435748])
    event("creature/beast_alert", [t(77637), t(389638)], [77637, 389638])
    event("creature/beast_flee", [pitch(t(389653), 1.1)], [389653])
    event("creature/bug_alert", grab(400663, 2, 0.05, 0.8, length=0.5), [400663])
    event("creature/bug_flee", [t(390733)], [390733])
    save()
