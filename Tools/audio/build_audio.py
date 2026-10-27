# Builds Data/Audio/*.ogg from the downloaded CC0 / CC-BY sources: trims, mono for 3D effects, resamples to 44.1 kHz,
# normalises loudness (short sounds by their loudest 400 ms, beds and music by integrated loudness, BS.1770 K-weighted),
# makes beds loop seamlessly (equal-power crossfade of the tail into the head) and encodes Ogg Vorbis.
# Usage: python -I build_audio.py <work dir (dl/, dl2/ from fetch_sources.py)> <Data/Audio>
# Needs numpy, scipy and soundfile (libsndfile with Vorbis), e.g. in a venv: pip install numpy scipy soundfile py7zr.
import json, os, sys, math, glob
import numpy as np, soundfile as sf
from scipy.signal import lfilter, resample_poly

ROOT, OUT = sys.argv[1], sys.argv[2]
DL, DL2 = os.path.join(ROOT, "dl"), os.path.join(ROOT, "dl2")
SR = 44100
rng = np.random.default_rng(7)

def kenney(pack, name):
    return os.path.join(DL, "kenney_" + pack, "x", "Audio", name)

def src(key, rel):
    hits = glob.glob(os.path.join(glob.escape(os.path.join(DL2, key)), "**", glob.escape(rel)), recursive=True)
    hits = [h for h in hits if "__MACOSX" not in h]
    if not hits:
        raise FileNotFoundError(key + ":" + rel)
    return hits[0]

# ---------------------------------------------------------------- loudness
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

def blocks(x, sr):
    y = kweight(x, sr)
    blk, hop = int(0.4 * sr), int(0.1 * sr)
    if len(y) < blk:
        return np.array([np.mean(np.sum(y ** 2, axis=1))])
    return np.array([np.mean(np.sum(y[i:i + blk] ** 2, axis=1)) for i in range(0, len(y) - blk + 1, hop)])

def lufs_integrated(x, sr):
    ms = blocks(x, sr); l = -0.691 + 10 * np.log10(ms + 1e-12)
    g = ms[l > -70]
    if len(g) == 0: return -70.0
    rel = -0.691 + 10 * np.log10(np.mean(g)) - 10
    g2 = ms[(l > -70) & (l > rel)]
    return float(-0.691 + 10 * np.log10(np.mean(g2) + 1e-12))

def lufs_max_momentary(x, sr):
    ms = blocks(x, sr)
    return float(-0.691 + 10 * np.log10(ms.max() + 1e-12))

# ---------------------------------------------------------------- shaping
def load(path):
    x, sr = sf.read(path, always_2d=True, dtype="float64")
    if sr != SR:
        g = math.gcd(SR, sr)
        x = resample_poly(x, SR // g, sr // g, axis=0)
    return x

def mono(x):
    return x.mean(axis=1, keepdims=True)

def stereo(x):
    if x.shape[1] >= 2: return x[:, :2]
    # Pseudo-stereo for mono beds: the right ear a few ms late (wide, but sums back to mono cleanly enough).
    d = int(0.017 * SR)
    r = np.concatenate([x[-d:, 0], x[:-d, 0]])
    return np.stack([x[:, 0], r], axis=1)

def cut(x, a, b=None):
    return x[int(a * SR): (int(b * SR) if b is not None else None)]

def trim(x, db=-50.0, pad=0.01):
    m = np.abs(x).max(axis=1)
    th = m.max() * 10 ** (db / 20)
    idx = np.where(m > th)[0]
    if len(idx) == 0: return x
    a = max(0, idx[0] - int(pad * SR)); b = min(len(x), idx[-1] + int(pad * SR * 4))
    return x[a:b]

def fade(x, fin=0.005, fout=0.02):
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

def segments(x, thresh_db=-38, min_sil=0.12, min_len=0.08):
    m = np.abs(x).max(axis=1); hop = int(SR * 0.01); n = len(m) // hop
    env = np.array([m[i * hop:(i + 1) * hop].max() for i in range(n)])
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

def onsets(x, min_gap=0.28, length=0.32):
    m = np.abs(x).max(axis=1); hop = int(SR * 0.005)
    env = np.array([m[i:i + hop].max() for i in range(0, len(m) - hop, hop)])
    env = np.convolve(env, np.ones(4) / 4, mode="same")
    th = env.max() * 0.25; out = []
    for i in range(1, len(env) - 1):
        if env[i] > th and env[i] >= env[i - 1] and env[i] > env[i + 1]:
            t = i * 0.005
            if not out or t - out[-1] > min_gap: out.append(t)
    return [(max(0, t - 0.03), t + length) for t in out]

def limit(x, ceiling_db=-1.0, release=0.05, look=0.002):
    """A peak limiter with a short look-ahead: transients are pulled under the ceiling instead of the whole sound."""
    thr = 10 ** (ceiling_db / 20)
    a = np.abs(x).max(axis=1)
    need = np.minimum(1.0, thr / np.maximum(a, 1e-9))
    from scipy.ndimage import minimum_filter1d
    n = max(1, int(look * SR))
    need = minimum_filter1d(need, size=2 * n + 1, mode="nearest")
    g = np.empty_like(need); cur = 1.0; k = 1.0 / (release * SR)
    for i in range(len(need)):
        cur = min(need[i], cur + (1.0 - cur) * k)
        g[i] = cur
    return x * g[:, None]

def normalize_short(x, target, max_limit_db=10.0, ceiling_db=-1.0):
    """Short sounds: loudest 400 ms to the target, transients limited by up to max_limit_db (clicks and footsteps
    have a high crest factor); what's still missing is left to the table's volume (see make_table.py)."""
    pk0 = np.abs(x).max() + 1e-9
    for _ in range(3):
        L = lufs_max_momentary(x, SR)
        g = 10 ** ((target - L) / 20)
        pk = np.abs(x).max() * g
        cap = 10 ** ((ceiling_db + max_limit_db) / 20)
        if pk > cap: g *= cap / pk
        x = limit(x * g, ceiling_db)
        if abs(lufs_max_momentary(x, SR) - target) < 0.3: break
    return x

def normalize(x, target, mode, peak_db=-1.0):
    L = lufs_max_momentary(x, SR) if mode == "momentary" else lufs_integrated(x, SR)
    g = 10 ** ((target - L) / 20)
    y = x * g
    pk = np.abs(y).max()
    lim = 10 ** (peak_db / 20)
    if pk > lim: y = y * (lim / pk)
    return y

# ---------------------------------------------------------------- output
manifest = {}
def write(name, x, kind, sources, quality=None):
    path = os.path.join(OUT, name + ".ogg")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if kind in ("sfx", "ui", "voice", "moment"):
        target = {"sfx": -16.0, "voice": -16.0, "ui": -18.0, "moment": -16.0}[kind]
        x = fade(normalize_short(x, target))
        q = quality or 0.55
    elif kind == "bed":
        x = normalize(x, -22.0, "integrated", -1.0)
        q = quality or 0.75
    else:  # music
        x = normalize(x, -18.0, "integrated", -0.5)
        q = quality or 0.78
    # compression_level 0..1: libsndfile maps it to Vorbis quality 1 - level.
    # Written in blocks: libsndfile's Vorbis encoder overflows the stack on one big write. Lossy encoding can push
    # peaks back over the limiter's ceiling, so the result is decoded and turned down until it stays under 0.95.
    for _ in range(4):
        data = np.clip(x, -1, 1).astype(np.float32)
        with sf.SoundFile(path, "w", SR, data.shape[1], format="OGG", subtype="VORBIS", compression_level=q) as f:
            for i in range(0, len(data), 4096):
                f.write(data[i:i + 4096])
        back, _sr = sf.read(path, dtype="float32", always_2d=True)
        pk = float(np.abs(back).max())
        if pk < 0.95: break
        x = x * (0.9 / pk)
    manifest[name] = {"sources": sorted(set(sources)), "seconds": round(len(x) / SR, 2), "channels": x.shape[1],
                      "bytes": os.path.getsize(path), "lufs": round(lufs_max_momentary(x, SR) if kind not in ("bed", "music") else lufs_integrated(x, SR), 1)}

def sfx(name, path, key, kind="sfx", seg=None, keep_stereo=False, gain_trim=True):
    x = load(path)
    if seg: x = cut(x, *seg)
    if not keep_stereo: x = mono(x)
    if gain_trim: x = trim(x)
    write(name, x, kind, [key])

def concat(name, parts, key_list, kind="sfx", gap=0.08):
    xs = []
    for p in parts:
        x = trim(mono(load(p)))
        xs.append(x); xs.append(np.zeros((int(gap * SR), 1)))
    write(name, np.concatenate(xs[:-1]), kind, key_list)

# ================================================================ footsteps
for i in range(5):
    sfx(f"foot/grass_{i+1}", kenney("impact-sounds", f"footstep_grass_00{i}.ogg"), "kenney_impact")
    sfx(f"foot/snow_{i+1}", kenney("impact-sounds", f"footstep_snow_00{i}.ogg"), "kenney_impact")
    sfx(f"foot/stone_{i+1}", kenney("impact-sounds", f"footstep_concrete_00{i}.ogg"), "kenney_impact")
    sfx(f"foot/wood_{i+1}", kenney("impact-sounds", f"footstep_wood_00{i}.ogg"), "kenney_impact")
for i in range(6):
    sfx(f"foot/gravel_{i+1}", kenney("rpg-audio", f"footstep0{i}.ogg"), "kenney_rpg")
sand = load(src("f_sand", "sand_footsteps_0.mp3"))
for i, (a, b) in enumerate(onsets(sand)[:6]):
    write(f"foot/sand_{i+1}", trim(mono(cut(sand, a, b))), "sfx", ["f_sand"])
for i, f in enumerate(["sfx100v2_footstep_wet_01.ogg", "sfx100v2_footstep_wet_02.ogg", "sfx100v2_footstep_wet_03.ogg"]):
    sfx(f"foot/water_{i+1}", src("f_sfx100", f), "f_sfx100")
for i, f in enumerate(["splash_09.ogg", "splash_10.ogg"]):
    sfx(f"foot/water_{i+4}", src("f_water", f), "f_water")

# ================================================================ combat
for i, n in enumerate([1, 2, 4, 5, 6, 8]):
    sfx(f"combat/swing_light_{i+1}", src("w_swish", f"swish-{n}.wav"), "w_swish")
for i, f in enumerate(["swish_2.wav", "swish_3.wav", "swish_4.wav"]):
    sfx(f"combat/swing_heavy_{i+1}", src("w_battle", f), "w_battle")
sfx("combat/swing_heavy_4", src("w_swish", "swish-9.wav"), "w_swish")
for i in range(5):
    sfx(f"combat/hit_flesh_{i+1}", kenney("impact-sounds", f"impactPunch_medium_00{i}.ogg"), "kenney_impact")
    sfx(f"combat/hit_armor_{i+1}", kenney("impact-sounds", f"impactMetal_medium_00{i}.ogg"), "kenney_impact")
for i in range(2):
    sfx(f"combat/hit_flesh_{i+6}", kenney("impact-sounds", f"impactPunch_heavy_00{i}.ogg"), "kenney_impact")
for i in range(3):
    sfx(f"combat/hit_block_{i+1}", kenney("impact-sounds", f"impactWood_heavy_00{i}.ogg"), "kenney_impact")
sfx("combat/hit_block_4", kenney("impact-sounds", "impactPlate_heavy_000.ogg"), "kenney_impact")
for i, n in enumerate([1, 3, 5, 6]):
    sfx(f"combat/parry_{i+1}", src("w_clash", f"sword_clash.{n}.ogg"), "w_clash")
for i in range(4):
    sfx(f"combat/dodge_{i+1}", kenney("rpg-audio", f"cloth{i+1}.ogg"), "kenney_rpg")
sfx("combat/jump_1", kenney("rpg-audio", "clothBelt.ogg"), "kenney_rpg")
sfx("combat/jump_2", kenney("rpg-audio", "clothBelt2.ogg"), "kenney_rpg")
sfx("combat/jump_3", src("w_leather", "step_lth1.ogg"), "w_leather")
for i in range(4):
    sfx(f"combat/land_{i+1}", kenney("impact-sounds", f"impactSoft_heavy_00{i}.ogg"), "kenney_impact")

# ================================================================ voices
for i in range(10):
    sfx(f"voice/hurt_{i+1}", src("v_male", f"slightscream-{i+1:02d}.flac"), "v_male", "voice")
grunts = load(src("v_maledeath", "death pain grunts.wav"))
gsegs = segments(grunts)
longs = sorted(gsegs, key=lambda s: s[1] - s[0], reverse=True)
for i, (a, b) in enumerate(longs[:4]):
    write(f"voice/death_{i+1}", trim(mono(cut(grunts, a - 0.02, b + 0.15))), "voice", ["v_maledeath"])
shorts = [s for s in gsegs if s[1] - s[0] < 1.0][:3]
for i, (a, b) in enumerate(shorts):
    write(f"voice/hurt_{i+11}", trim(mono(cut(grunts, a - 0.02, b + 0.1))), "voice", ["v_maledeath"])
for i in range(3):
    sfx(f"voice/hurt_female_{i+1}", src("v_female", f"Type 1/damaged{i+1}.wav"), "v_female", "voice")
fem = load(src("v_femalehurt", "female_hurt_grunts_groans_1.ogg"))
fsegs = segments(fem)
flong = sorted(fsegs, key=lambda s: s[1] - s[0], reverse=True)
for i, (a, b) in enumerate(flong[:2]):
    write(f"voice/death_female_{i+1}", trim(mono(cut(fem, a - 0.02, b + 0.15))), "voice", ["v_femalehurt"])
for i, (a, b) in enumerate([s for s in fsegs if s not in flong[:2]][:4]):
    write(f"voice/hurt_female_{i+4}", trim(mono(cut(fem, a - 0.02, b + 0.1))), "voice", ["v_femalehurt"])

# ================================================================ items
for i, n in enumerate([1, 2, 4, 5]):
    sfx(f"item/eat_{i+1}", src("f_crunch", f"crunch.{n}.ogg"), "f_crunch")
for i, n in enumerate([1, 3, 4, 5]):
    sfx(f"item/drink_{i+1}", src("f_drink", f"swallow-0{n}.flac"), "f_drink")
concat("item/potion_1", [src("f_drink", "bottle-open-03.flac"), src("f_drink", "swallow-04.flac")], ["f_drink"])
concat("item/potion_2", [src("f_drink", "bottle-open-01.flac"), src("f_drink", "swallow-05.flac")], ["f_drink"])
for i, f in enumerate(["handleSmallLeather.ogg", "handleSmallLeather2.ogg", "beltHandle1.ogg", "beltHandle2.ogg"]):
    sfx(f"item/pickup_{i+1}", kenney("rpg-audio", f), "kenney_rpg")
for i, f in enumerate(["drawKnife1.ogg", "drawKnife2.ogg", "drawKnife3.ogg", "metalLatch.ogg"]):
    sfx(f"item/equip_{i+1}", kenney("rpg-audio", f), "kenney_rpg")

# ================================================================ ranged
for i in range(3):
    sfx(f"ranged/bow_draw_{i+1}", kenney("rpg-audio", f"creak{i+1}.ogg"), "kenney_rpg")
sfx("ranged/bow_release_1", src("w_archery", "0549.ogg"), "w_archery")
sfx("ranged/bow_release_2", src("w_battle", "Bow.wav"), "w_battle")
sfx("ranged/crossbow_1", src("w_crossbowdry", "crossbow_dryshot.flac"), "w_crossbow")
sfx("ranged/blowgun_1", src("f_sfx100", "sfx100v2_air_03.ogg"), "f_sfx100")
air = trim(mono(load(src("f_sfx100", "sfx100v2_air_02.ogg"))))
write("ranged/blowgun_2", cut(air, 0, 0.6), "sfx", ["f_sfx100"])
sfx("ranged/throw_1", src("w_swish", "swish-7.wav"), "w_swish")
sfx("ranged/throw_2", src("w_battle", "swish_3.wav"), "w_battle")
arrow = load(src("w_arrowhit", "0548.ogg"))
for i, (a, b) in enumerate(segments(arrow)[:3]):
    write(f"ranged/arrow_hit_{i+1}", trim(mono(cut(arrow, a - 0.01, b + 0.25))), "sfx", ["w_arrowhit"])

# ================================================================ spells
for i, n in enumerate([3, 4, 6]):
    sfx(f"spell/cast_fire_{i+1}", src("s_rpg", f"spell_fire_0{n}.ogg"), "s_rpg")
sfx("spell/cast_frost_1", src("s_ice", "coldsnap.wav"), "s_ice")
sfx("spell/cast_frost_2", src("s_ice", "ice.wav"), "s_ice")
for i, n in enumerate([2, 7, 8]):
    sfx(f"spell/fire_impact_{i+1}", src("s_bang", f"bang_0{n}.ogg"), "s_bang")
for i, f in enumerate(["LedasLuzta33.ogg", "LedasLuzta4.ogg", "LedasLuzta5.ogg", "LedasLuzta2.ogg"]):
    sfx(f"spell/frost_impact_{i+1}", src("s_shatter", f), "s_shatter")
lava = load(src("a_lava", "LavaLoop.wav"))
write("spell/lavapool_1", fade(mono(cut(lava, 0.0, 2.6)), 0.05, 0.6), "sfx", ["a_lava"])
write("spell/lavapool_2", fade(mono(cut(lava, 8.0, 10.6)), 0.05, 0.6), "sfx", ["a_lava"])

# ================================================================ world
for i in range(5):
    sfx(f"world/gather_wood_{i+1}", kenney("impact-sounds", f"impactWood_medium_00{i}.ogg"), "kenney_impact")
    sfx(f"world/gather_stone_{i+1}", kenney("impact-sounds", f"impactMining_00{i}.ogg"), "kenney_impact")
    sfx(f"world/build_place_{i+1}", kenney("impact-sounds", f"impactPlank_medium_00{i}.ogg"), "kenney_impact")
sfx("world/gather_wood_6", kenney("rpg-audio", "chop.ogg"), "kenney_rpg")
sfx("world/build_demolish_1", src("f_woodmetal", "wood_breaking_01.ogg"), "f_woodmetal")
sfx("world/build_demolish_2", src("f_woodmetal", "wood_breaking_02.ogg"), "f_woodmetal")
sfx("world/build_demolish_3", src("f_break", "bfh1_breaking_01.ogg"), "f_break")
sfx("world/build_demolish_4", src("f_break", "bfh1_rock_breaking_01.ogg"), "f_break")
for i in range(2):
    sfx(f"world/door_open_{i+1}", kenney("rpg-audio", f"doorOpen_{i+1}.ogg"), "kenney_rpg")
for i in range(4):
    sfx(f"world/door_close_{i+1}", kenney("rpg-audio", f"doorClose_{i+1}.ogg"), "kenney_rpg")
sfx("world/craft_done_1", src("f_anvil", "forger_0.ogg"), "f_anvil")
sfx("world/craft_done_2", src("f_smith", "blacksmithhammer.wav"), "f_smith")
crackle = mono(load(src("a_crackle", "fire-1_0.ogg")))
flame = trim(mono(load(src("f_ignite", "flame_0.ogg"))))
feed = np.concatenate([flame, np.zeros((int(0.05 * SR), 1))])
tail = fade(cut(crackle, 0.3, 1.8), 0.05, 0.6) * 0.5
mix = np.zeros((len(feed) + len(tail) - int(0.25 * SR), 1)); mix[:len(feed)] += feed; mix[-len(tail):] += tail
write("world/hearth_feed_1", mix, "sfx", ["f_ignite", "a_crackle"])

# ================================================================ moments (2D)
J = lambda n: kenney("music-jingles", f"Pizzicato jingles/jingles_PIZZI{n:02d}.ogg")
sfx("moment/levelup", J(2), "kenney_jingles", "moment", keep_stereo=True)
sfx("moment/recipe_unlock", J(10), "kenney_jingles", "moment", keep_stereo=True)
sfx("moment/discover", J(12), "kenney_jingles", "moment", keep_stereo=True)
sfx("moment/tame_success", J(15), "kenney_jingles", "moment", keep_stereo=True)
boss = load(src("m_bossintro", "Heavy Concept A Bass Master.wav"))
write("moment/boss_intro", fade(cut(boss, 0, 7.5), 0.01, 2.0), "moment", ["m_bossintro"])
sfx("moment/respawn", src("s_magic", "magical_1_0.ogg"), "s_magic", "moment", keep_stereo=True)

# ================================================================ UI (2D)
sfx("ui/hover_1", kenney("interface-sounds", "tick_004.ogg"), "kenney_interface", "ui")
sfx("ui/hover_2", kenney("ui-audio", "rollover2.ogg"), "kenney_ui", "ui")
sfx("ui/click_2", kenney("ui-audio", "click1.ogg"), "kenney_ui", "ui")
sfx("ui/click_3", kenney("ui-audio", "click3.ogg"), "kenney_ui", "ui")
sfx("ui/open_1", kenney("rpg-audio", "bookOpen.ogg"), "kenney_rpg", "ui")
sfx("ui/open_2", kenney("rpg-audio", "bookFlip2.ogg"), "kenney_rpg", "ui")
sfx("ui/close_1", kenney("rpg-audio", "bookClose.ogg"), "kenney_rpg", "ui")
sfx("ui/close_2", kenney("rpg-audio", "bookPlace1.ogg"), "kenney_rpg", "ui")
sfx("ui/error_1", kenney("interface-sounds", "error_004.ogg"), "kenney_interface", "ui")
sfx("ui/error_2", kenney("interface-sounds", "error_006.ogg"), "kenney_interface", "ui")
sfx("ui/drag_1", kenney("interface-sounds", "pluck_001.ogg"), "kenney_interface", "ui")
sfx("ui/drag_2", kenney("interface-sounds", "pluck_002.ogg"), "kenney_interface", "ui")
for i in range(3):
    sfx(f"ui/drop_{i+1}", kenney("interface-sounds", f"drop_00{i+1}.ogg"), "kenney_interface", "ui")

# ================================================================ creatures
def cr(name, key, rel, seg=None):
    sfx("creature/" + name, src(key, rel), key, seg=seg)
for i, (k, f) in enumerate([("c_rd1", "howl.ogg"), ("c_wolf", "wolf_monster_6.mp3"), ("c_rd1", "barking_01.ogg"), ("c_rd1", "barking_02.ogg")]):
    cr(f"canine_idle_{i+1}", k, f)
for i, n in enumerate([2, 3, 4, 5, 9]):
    cr(f"canine_aggro_{i+1}", "c_growls", f"monster.{n}.ogg")
for i in range(4):
    cr(f"canine_attack_{i+1}", "c_rd2", f"attack_0{i+1}.ogg")
for i in range(5):
    cr(f"canine_hurt_{i+1}", "c_rd1", f"hurt_0{i+1}.ogg")
for i in range(3):
    cr(f"canine_death_{i+1}", "c_rd2", f"die_0{i+1}.ogg")
troll_idle = load(src("c_trollidle", "troll-idle-noises_0.ogg")); tis = segments(troll_idle)
troll_roar = load(src("c_trollroar", "troll-roars_0.ogg")); trs = segments(troll_roar)
mino = load(src("c_mino", "mino1.wav")); ms_ = segments(mino)
cr("beast_idle_1", "c_bear", "bear_01.ogg"); cr("beast_idle_2", "c_bear", "bear_02.ogg")
write("creature/beast_idle_3", trim(mono(cut(troll_idle, *tis[2]))), "sfx", ["c_trollidle"])
for i, f in enumerate(["roar_02.ogg", "roar_03.ogg"]):
    cr(f"beast_aggro_{i+1}", "c_rd1", f)
for i in range(3):
    cr(f"beast_aggro_{i+3}", "s_rpg", f"creature_roar_0{i+1}.ogg")
for i, n in enumerate([1, 3, 6, 7]):
    cr(f"beast_attack_{i+1}", "c_rd1", f"monster_0{n}.ogg")
write("creature/beast_attack_5", trim(mono(cut(mino, ms_[0][0] - 0.02, ms_[0][1] + 0.1))), "sfx", ["c_mino"])
for i, n in enumerate([1, 3, 4]):
    cr(f"beast_hurt_{i+1}", "c_rd1", f"grunt_0{n}.ogg")
cr("beast_hurt_4", "s_rpg", "creature_hurt_01.ogg"); cr("beast_hurt_5", "s_rpg", "creature_hurt_02.ogg")
cr("beast_death_1", "s_rpg", "creature_die_01.ogg"); cr("beast_death_2", "c_rd2", "die_04.ogg")
write("creature/beast_death_3", trim(mono(cut(mino, ms_[1][0] - 0.02, ms_[1][1] + 0.2))), "sfx", ["c_mino"])
for i in range(3):
    cr(f"flyer_call_{i+1}", "c_bat", f"bat_0{i+1}.ogg")
cr("flyer_scream_1", "c_rd1", "scream_01.ogg"); cr("flyer_scream_2", "c_rd1", "scream_02.ogg")
cr("flyer_attack_1", "c_rd2", "attack_05.ogg")
for i in range(4):
    cr(f"bug_idle_{i+1}", "c_rd1", f"bug_0{i+1}.ogg")
for i, n in enumerate([5, 6, 7, 8]):
    cr(f"bug_aggro_{i+1}", "c_rd2", f"bug_0{n}.ogg")
for i, n in enumerate([9, 10, 11, 12, 13]):
    cr(f"bug_attack_{i+1}", "c_rd2", f"bug_{n:02d}.ogg")
for i, n in enumerate([7, 9, 10]):
    cr(f"bug_hurt_{i+1}", "c_rd2", f"alien_{n:02d}.ogg")
cr("bug_death_1", "c_insect", "insectoralien.flac"); cr("bug_death_2", "c_rd1", "alien_01.ogg")
for i, (k, f) in enumerate([("c_rd1", "nose.ogg"), ("c_rd1", "breath.ogg"), ("c_rd2", "grunt_06.ogg"), ("c_rd2", "grunt_07.ogg"), ("c_rd2", "grunt_09.ogg")]):
    cr(f"herd_idle_{i+1}", k, f)
for i in range(2):
    write(f"creature/herd_aggro_{i+1}", trim(mono(cut(troll_roar, *trs[i]))), "sfx", ["c_trollroar"])
for i, (k, f) in enumerate([("c_rd2", "stomp_01.ogg"), ("c_rd1", "grunt_05.ogg"), ("c_rd2", "grunt_08.ogg")]):
    cr(f"herd_attack_{i+1}", k, f)
for i, (k, f) in enumerate([("c_rd1", "grunt_02.ogg"), ("c_ogre", "Monster-4.wav"), ("c_ogre", "Monster-5.wav")]):
    cr(f"herd_hurt_{i+1}", k, f)
cr("herd_death_1", "c_rd2", "die_02.ogg"); cr("herd_death_2", "c_ogre", "monster-14.wav")
for i in range(10):
    cr(f"critter_{i+1}", "c_rd1", f"cute_{i+1:02d}.ogg")
pig = load(src("c_pig", "1659.ogg")); ps = segments(pig)
for i, j in enumerate([0, 2, 3, 4, 7]):
    write(f"creature/pig_{i+1}", trim(mono(cut(pig, ps[j][0] - 0.02, ps[j][1] + 0.05))), "sfx", ["c_pig"])
for i in range(3):
    sfx(f"creature/wisp_chime_{i+1}", kenney("interface-sounds", f"glass_00{i+1}.ogg"), "kenney_interface")
sfx("creature/wisp_chime_4", kenney("interface-sounds", "glass_004.ogg"), "kenney_interface")
sfx("creature/wisp_chime_5", kenney("interface-sounds", "glass_006.ogg"), "kenney_interface")
cr("wisp_spell_1", "s_rpg", "spell_01.ogg"); cr("wisp_spell_2", "s_rpg", "spell_02.ogg")
cr("wisp_magic_1", "s_magic", "magical_1_0.ogg")
for i, j in enumerate([1, 6]):
    write(f"creature/boss_idle_{i+1}", trim(mono(cut(troll_idle, *tis[j]))), "sfx", ["c_trollidle"])
cr("boss_roar_1", "c_roar", "monster_roar.wav")
write("creature/boss_roar_2", trim(mono(cut(troll_roar, *trs[4]))), "sfx", ["c_trollroar"])
for i, j in enumerate([0, 2, 3, 5]):
    write(f"creature/boss_attack_{i+1}", trim(mono(cut(troll_roar, *trs[j]))), "sfx", ["c_trollroar"])
for i in range(3):
    cr(f"boss_hurt_{i+1}", "c_ogre", f"Monster-{i+1}.wav")
for i in range(4):
    cr(f"golem_stones_{i+1}", "s_rpg", f"stones_0{i+1}.ogg")
for i, n in enumerate([4, 5, 6]):
    cr(f"golem_roar_{i+1}", "c_rd2", f"roar_0{n}.ogg")
cr("golem_death_1", "f_cavein", "cave_in_-_starninjas_0.ogg")
for i in range(8):
    cr(f"human_{i+1}", "c_rd2", f"human_0{i+1}.ogg")

# ================================================================ ambient beds (2D streams) and owl spots
def bed(name, x, key, xfade, secs=None, start=0.0, quality=None):
    x = stereo(x)
    x = cut(x, start, start + secs + xfade) if secs else cut(x, start)
    write("amb/" + name, loop_xfade(x, xfade), "bed", key if isinstance(key, list) else [key], quality)
bed("volcanic_rumble", load(src("a_rumble", "low-rumbling-176033.mp3")), "a_rumble", 3.0)
# Crackle bed: grains of the crackle recording scattered over 40 s (wrapped, so it loops), at random levels and pans.
grain_src = mono(load(src("a_crackle", "fire-1_0.ogg")))[:, 0]
N = 40 * SR; bedx = np.zeros((N, 2))
t = 0.0
while t < 40.0:
    L = rng.uniform(0.15, 0.7); a = rng.uniform(0, len(grain_src) / SR - L)
    g = grain_src[int(a * SR): int((a + L) * SR)].copy(); n = len(g)
    env = np.ones(n); f = min(int(0.02 * SR), n // 2); env[:f] = np.linspace(0, 1, f); env[-f:] = np.linspace(1, 0, f)
    g *= env * 10 ** (rng.uniform(-14, 0) / 20); pan = rng.uniform(0.2, 0.8)
    s = int(t * SR); idx = (np.arange(n) + s) % N
    bedx[idx, 0] += g * (1 - pan); bedx[idx, 1] += g * pan
    t += rng.exponential(0.45)
write("amb/volcanic_crackle", bedx, "bed", ["a_crackle"])
bed("lava", load(src("a_lava", "LavaLoop.wav")), "a_lava", 1.0)
bed("tundra_wind", load(src("a_wind", "0146.ogg")), "a_wind", 4.0, secs=58.0, start=5.0)
bed("high_wind", load(src("a_whistle", "0147.ogg")), "a_whistle", 4.0, secs=48.0, start=10.0)
bed("coast_surf", load(src("a_seawaves", "0698.ogg")), "a_seawaves", 5.0, secs=70.0, start=20.0)
bed("day_birds", load(src("a_morning", "amb_morning_0.ogg")), "a_morning", 2.0)
cr_x = stereo(load(src("a_crickets", "crickets_1.mp3")))
ins = stereo(load(src("a_insect", "0426.ogg")))
night = np.concatenate([cr_x, cr_x[int(3.1 * SR):], cr_x[int(1.7 * SR):]])
lay = ins[:len(night)] if len(ins) >= len(night) else np.concatenate([ins, ins])[:len(night)]
night = night + lay * 10 ** (-16 / 20) * (np.sqrt(np.mean(cr_x ** 2)) / (np.sqrt(np.mean(ins ** 2)) + 1e-9))
write("amb/night_insects", loop_xfade(night, 1.5), "bed", ["a_crickets", "a_insect"])
bed("cave", load(src("a_dungeon", "dungeon_ambient_1_0.ogg")), "a_dungeon", 4.0, secs=60.0)
owls = load(src("a_owls", "0429.ogg")); os_ = segments(owls)
picked = [s for s in os_ if 0.4 <= s[1] - s[0] <= 2.6][:5]
for i, (a, b) in enumerate(picked):
    write(f"amb/owl_{i+1}", trim(mono(cut(owls, a - 0.02, b + 0.3))), "sfx", ["a_owls"])

# ================================================================ music (2D streams)
def track(name, key, rel, secs=None, fin=1.5, fout=5.0, quality=None):
    x = stereo(load(src(key, rel)))
    if secs: x = cut(x, 0, secs)
    x = fade(x, fin, fout) if (fin or fout) else x
    write("music/" + name, x, "music", [key], quality)
track("menu", "m_menu", "FantasyOrchestralTheme_1.mp3", fin=1.0, fout=4.0)
track("explore_winter", "m_winter", "Long Winter_0.mp3", secs=150)
track("explore_winds", "m_winds", "mysteriouswinds.mp3")
track("explore_lands", "m_explore", "Exploration_0.mp3", secs=150)
track("night_moon", "m_moon", "Rising_Moon_0.mp3", secs=150)
track("night_cathedral", "m_cathedral", "forest_2_0.ogg", fin=0, fout=0)
track("combat", "m_battle", "battleThemeA.mp3", fin=0.02, fout=0.3)
track("boss", "m_boss", "Juhani Junkala - Epic Boss Battle [Seamlessly Looping].wav", fin=0, fout=0)

with open(os.path.join(ROOT, "audio_manifest.json"), "w") as f:  # make_table.py and credits.py read it
    json.dump(manifest, f, indent=1)
total = sum(m["bytes"] for m in manifest.values())
print(f"{len(manifest)} files, {total / 1048576:.2f} MB")
for k in ["amb", "music", "creature", "foot", "combat", "voice", "ui", "spell", "world", "item", "ranged", "moment"]:
    b = sum(m["bytes"] for n, m in manifest.items() if n.startswith(k + "/"))
    print(f"  {k}: {sum(1 for n in manifest if n.startswith(k + '/'))} files {b / 1048576:.2f} MB")
