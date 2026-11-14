"""The raid sounds, synthesised (cooks/build-server-raids.md 2.10): our own, no source files, CC0 like the rest we make.
  raid/bell_1..3     a bronze bell struck: campanology partials hum 0.5, prime 1, tierce 1.2 (minor third), quint 1.5,
                     nominal 2, superquint 3, octave nominal 4, each with its own decay (higher dies faster), a metallic
                     strike transient, slight detune beats; three strikes of different strength and pitch.
  raid/blast_1..2    a black-powder blast: a sub thump (a falling sine), a broadband crack, a rumbling low noise tail
                     and debris rattle (grit falling), 4.5 s.
  raid/fuse          a slow match burning: hiss (high-passed noise) with crackle pops, 5 s, loops cleanly.
  raid/codebreaker   the resonant fork on a lock: two close tones beating (4 Hz) over a bright ring, 4 s.
  raid/pot_break     a clay jar breaking and fire catching: a sharp crack, shard tinkles, a whoomp.
Run: python Tools/audio/synth_raid.py   (needs numpy and ffmpeg on the PATH). Writes Data/Audio/raid/*.ogg (mono 44.1 kHz)."""
import os, subprocess, tempfile, wave
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'Data', 'Audio', 'raid')
SR = 44100
RNG = np.random.default_rng(1871)


def t_axis(sec):
    return np.arange(int(SR * sec)) / SR


def lowpass(x, cutoff):
    a = np.exp(-2.0 * np.pi * cutoff / SR)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def write(name, x, gain_db=-1.0):
    x = x / (np.max(np.abs(x)) + 1e-9) * (10 ** (gain_db / 20.0))
    os.makedirs(OUT, exist_ok=True)
    tmp = os.path.join(tempfile.gettempdir(), 'wr_raid_%s.wav' % name)
    with wave.open(tmp, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype(np.int16).tobytes())
    dst = os.path.join(OUT, name + '.ogg')
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', tmp, '-c:a', 'libvorbis', '-q:a', '4', dst], check=True)
    os.remove(tmp)
    print(dst, os.path.getsize(dst))


def bell(fundamental, strength, seconds=5.0):
    t = t_axis(seconds)
    # (ratio, level, decay seconds): hum longest, the upper partials short.
    partials = [(0.5, 0.55, 4.5), (1.0, 0.8, 3.2), (1.19, 0.6, 2.4), (1.5, 0.35, 2.0), (2.0, 1.0, 1.8),
                (2.51, 0.3, 1.1), (3.0, 0.4, 0.9), (4.0, 0.25, 0.6), (5.33, 0.12, 0.35)]
    x = np.zeros_like(t)
    for r, lvl, dec in partials:
        f = fundamental * r
        beat = 1.0 + 0.0015 * RNG.uniform(-1, 1)
        x += lvl * np.exp(-t / (dec * (0.8 + 0.4 * strength))) * (np.sin(2 * np.pi * f * t) + 0.35 * np.sin(2 * np.pi * f * beat * t + 1.3))
    # The strike: a few ms of bright noise and a click (iron on bronze).
    n = int(0.012 * SR)
    hit = highpass(RNG.standard_normal(n), 2500) * np.exp(-np.arange(n) / (0.003 * SR))
    x[:n] += hit * (1.5 + strength)
    x *= np.minimum(1.0, t / 0.002)
    return x


def blast(seconds=4.5, pitch=1.0):
    t = t_axis(seconds)
    f = 55 * pitch * np.exp(-t * 2.2) + 22
    phase = 2 * np.pi * np.cumsum(f) / SR
    thump = np.sin(phase) * np.exp(-t / 0.35)
    crack_n = int(0.06 * SR)
    crack = np.zeros_like(t)
    crack[:crack_n] = RNG.standard_normal(crack_n) * np.exp(-np.arange(crack_n) / (0.012 * SR))
    rumble = lowpass(RNG.standard_normal(len(t)), 160 * pitch) * np.exp(-t / 1.4) * 6.0
    body = lowpass(RNG.standard_normal(len(t)), 1800 * pitch) * np.exp(-t / 0.25) * 1.6
    debris = np.zeros_like(t)
    for _ in range(140):
        at = int(SR * RNG.uniform(0.25, seconds - 0.3))
        ln = int(SR * RNG.uniform(0.004, 0.02))
        if at + ln < len(t):
            debris[at:at + ln] += highpass(RNG.standard_normal(ln), 1500) * RNG.uniform(0.05, 0.3) * np.exp(-at / SR / 1.6)
    return thump * 1.4 + crack * 1.2 + rumble + body + debris


def fuse(seconds=5.0):
    t = t_axis(seconds)
    hiss = highpass(RNG.standard_normal(len(t)), 3000) * (0.55 + 0.15 * np.sin(2 * np.pi * 0.7 * t))
    pops = np.zeros_like(t)
    for _ in range(int(seconds * 14)):
        at = int(SR * RNG.uniform(0, seconds - 0.01))
        ln = int(SR * 0.004)
        pops[at:at + ln] += RNG.standard_normal(ln) * RNG.uniform(0.6, 1.6)
    x = hiss + pops
    # A clean loop: the last 50 ms fade into the first.
    f = int(0.05 * SR)
    x[-f:] = x[-f:] * np.linspace(1, 0, f) + x[:f] * np.linspace(0, 1, f)
    return x


def codebreaker(seconds=4.0):
    t = t_axis(seconds)
    base = 1318.5   # E6: a small fork's ring
    x = np.sin(2 * np.pi * base * t) + np.sin(2 * np.pi * (base + 4.0) * t) + 0.4 * np.sin(2 * np.pi * base * 2.76 * t) * np.exp(-t / 0.6)
    x += 0.25 * np.sin(2 * np.pi * 659.25 * t + 0.5)
    env = np.minimum(1.0, t / 0.03) * (0.75 + 0.25 * np.sin(2 * np.pi * 0.5 * t))
    f = int(0.08 * SR)
    env[-f:] *= np.linspace(1, 0, f)
    return x * env


def pot_break(seconds=1.6):
    t = t_axis(seconds)
    n = int(0.03 * SR)
    x = np.zeros_like(t)
    x[:n] = highpass(RNG.standard_normal(n), 1200) * np.exp(-np.arange(n) / (0.006 * SR)) * 2.0
    for _ in range(22):   # shards
        at = int(SR * RNG.uniform(0.02, 0.6))
        ln = int(SR * 0.03)
        f = RNG.uniform(2500, 6000)
        x[at:at + ln] += np.sin(2 * np.pi * f * np.arange(ln) / SR) * np.exp(-np.arange(ln) / (0.006 * SR)) * RNG.uniform(0.1, 0.4)
    whoomp = lowpass(RNG.standard_normal(len(t)), 500) * np.exp(-((t - 0.12) ** 2) / 0.02) * 5.0
    roar = lowpass(RNG.standard_normal(len(t)), 900) * np.exp(-t / 0.7) * np.minimum(1.0, t / 0.1) * 2.0
    return x + whoomp + roar


if __name__ == '__main__':
    write('bell_1', bell(392.0, 1.0))
    write('bell_2', bell(386.0, 0.7))
    write('bell_3', bell(398.0, 0.45), gain_db=-3.0)
    write('blast_1', blast(4.5, 1.0))
    write('blast_2', blast(4.5, 0.85))
    write('fuse', fuse(), gain_db=-6.0)
    write('codebreaker', codebreaker(), gain_db=-4.0)
    write('pot_break', pot_break(), gain_db=-2.0)
