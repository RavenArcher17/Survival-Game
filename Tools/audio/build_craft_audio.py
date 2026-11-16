# The crafting cook pass's sounds (cooks/cookpass-crafting.md 3.1-3.3): hammer on hot iron at the forge's anvil, the
# armourer's light planishing taps, knapping flint, a plane stroke, a pestle in a mortar, the quench, and the forge's coal
# bed roaring (a seamless loop). Recordings: Freesound, CC0 1.0 (licence read off each sound's page, 2026-11-16), the HQ
# preview MP3. Docs/CREDITS.md lists them. Cut points are the strikes' onsets (an RMS onset pass, 10 ms frames).
# Fetch:  python -I build_craft_audio.py fetch <work>        -> <work>/dl/<id>.mp3
# Build:  python -I build_craft_audio.py build <work> Data/Audio   (ffmpeg on PATH or FFMPEG=<path>; numpy for the loop)
import os, subprocess, sys, time, urllib.request

SOURCES = {
    856537: ("Artiom_Constantinov", "forging blade", "https://cdn.freesound.org/previews/856/856537_7254491-hq.mp3"),
    434339: ("draftcraft", "Hammer hitting an anvil", "https://cdn.freesound.org/previews/434/434339_6599148-hq.mp3"),
    450874: ("kyles", "tap hammer small piece of metal gentle hits", "https://cdn.freesound.org/previews/450/450874_612689-hq.mp3"),
    858891: ("xtra1", "Stone on Stone Hit", "https://cdn.freesound.org/previews/858/858891_18963922-hq.mp3"),
    798163: ("mathiaslyhne1", "9. Flint cutting 2", "https://cdn.freesound.org/previews/798/798163_4977295-hq.mp3"),
    379482: ("jandobes97", "hoblovani dreva (hand planing)", "https://cdn.freesound.org/previews/379/379482_7029322-hq.mp3"),
    749884: ("Fwughox_Official", "Mortar and pestle", "https://cdn.freesound.org/previews/749/749884_16187367-hq.mp3"),
    395864: ("jokallset", "Hot metal in water", "https://cdn.freesound.org/previews/395/395864_2808036-hq.mp3"),
    765947: ("SoundDesigner0101", "Gas Forge", "https://cdn.freesound.org/previews/765/765947_15695818-hq.mp3"),
    386150: ("ldezem", "Forge - Coal burning with fan on close", "https://cdn.freesound.org/previews/386/386150_1655965-hq.mp3"),
}

# output (under Data/Audio) -> (source id, start s, end s, target loudness LUFS)
CUTS = {
    "craft/anvil_1": (856537, 72.59, 73.25, -16),
    "craft/anvil_2": (856537, 74.74, 75.40, -16),
    "craft/anvil_3": (856537, 3.11, 3.77, -16),
    "craft/anvil_4": (434339, 5.72, 6.50, -16),
    "craft/tap_1": (450874, 4.41, 4.62, -19),
    "craft/tap_2": (450874, 2.86, 3.04, -19),
    "craft/tap_3": (450874, 1.06, 1.24, -19),
    "craft/knap_1": (858891, 7.97, 8.38, -19),
    "craft/knap_2": (858891, 3.49, 3.90, -19),
    "craft/knap_3": (798163, 2.90, 3.40, -20),
    "craft/plane_1": (379482, 4.82, 5.40, -20),
    "craft/plane_2": (379482, 6.50, 7.10, -20),
    "craft/grind_1": (749884, 7.37, 7.75, -21),
    "craft/grind_2": (749884, 8.11, 8.60, -21),
    "craft/grind_3": (749884, 9.76, 10.16, -21),
    "craft/quench_1": (395864, 24.25, 26.05, -19),
}

# A seamless loop: (output, source, start, end, crossfade s, LUFS, layer source, layer start, layer gain dB)
LOOPS = [("craft/forge_roar", 765947, 5.0, 15.5, 0.5, -22, 386150, 1.0, -8.0)]


def fetch(work):
    os.makedirs(os.path.join(work, "dl"), exist_ok=True)
    for sid, (_, _, url) in SOURCES.items():
        out = os.path.join(work, "dl", f"{sid}.mp3")
        if os.path.exists(out):
            continue
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=60) as r, open(out, "wb") as f:
            f.write(r.read())
        time.sleep(1)


def decode(ff, src, a, b, rate=44100):
    import numpy as np
    raw = subprocess.run([ff, "-v", "error", "-ss", str(a), "-to", str(b), "-i", src, "-ac", "1", "-ar", str(rate), "-f", "f32le", "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).copy()


def build_loop(ff, work, audio, row):
    import numpy as np
    name, sid, a, b, xf, lufs, lsid, la, lgain = row
    rate = 44100
    x = decode(ff, os.path.join(work, "dl", f"{sid}.mp3"), a, b, rate)
    layer = decode(ff, os.path.join(work, "dl", f"{lsid}.mp3"), la, la + (b - a), rate)
    n = min(len(x), len(layer)) if len(layer) > rate else len(x)
    if len(layer) >= n:
        x = x[:n] + layer[:n] * (10 ** (lgain / 20))
    # Equal-power crossfade of the tail into the head: the loop's end runs straight into its start.
    k = int(xf * rate)
    t = np.linspace(0, 1, k, dtype=np.float32)
    head, tail = x[:k], x[-k:]
    blended = tail * np.cos(t * np.pi / 2) + head * np.sin(t * np.pi / 2)
    y = np.concatenate([blended, x[k:-k]])
    tmp = os.path.join(work, name.replace("/", "_") + ".f32")
    y.astype(np.float32).tofile(tmp)
    dst = os.path.join(audio, name + ".ogg")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    subprocess.run([ff, "-hide_banner", "-loglevel", "error", "-y", "-f", "f32le", "-ar", str(rate), "-ac", "1", "-i", tmp,
                    "-af", f"highpass=f=40,loudnorm=I={lufs}:TP=-2:LRA=11,aresample=44100", "-c:a", "libvorbis", "-q:a", "4", dst], check=True)
    os.remove(tmp)
    print(dst)


def build(work, audio):
    ff = os.environ.get("FFMPEG", "ffmpeg")
    for name, (sid, a, b, lufs) in CUTS.items():
        src = os.path.join(work, "dl", f"{sid}.mp3")
        dst = os.path.join(audio, name + ".ogg")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        dur = b - a
        fade_in = min(0.004, dur / 10)
        fade = min(0.08, dur / 4)
        chain = (f"atrim={a}:{b},asetpts=PTS-STARTPTS,highpass=f=60,"
                 f"afade=t=in:d={fade_in},afade=t=out:st={max(0.0, dur - fade)}:d={fade},"
                 f"loudnorm=I={lufs}:TP=-2:LRA=11,aresample=44100")
        subprocess.run([ff, "-hide_banner", "-loglevel", "error", "-y", "-i", src, "-af", chain, "-ac", "1",
                        "-c:a", "libvorbis", "-q:a", "4", dst], check=True)
        print(dst)
    for row in LOOPS:
        build_loop(ff, work, audio, row)


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "fetch":
        fetch(sys.argv[2])
    elif len(sys.argv) >= 4 and sys.argv[1] == "build":
        build(sys.argv[2], sys.argv[3])
    else:
        print("fetch <work> | build <work> <Data/Audio>")
