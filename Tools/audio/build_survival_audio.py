# The survival cook pass's sounds (cooks/cookpass-survival.md 3.1-3.3): bites, chews and swallows for eating, the
# waterskin filling, the stomach and the dry throat. Recordings: Freesound, CC0 1.0 (licence read off each sound's page,
# 2026-11-15), the HQ preview MP3. Docs/CREDITS.md lists them.
# Fetch:  python -I build_survival_audio.py fetch <work>        -> <work>/dl/<id>.mp3
# Build:  python -I build_survival_audio.py build <work> Data/Audio   (ffmpeg on PATH or FFMPEG=<path>)
import os, subprocess, sys, time, urllib.request

SOURCES = {
    447911: ("Breviceps", "Growling stomach / Stomach rumbles", "https://cdn.freesound.org/previews/447/447911_9159316-hq.mp3"),
    696025: ("UnderASpell", "Stomach Groaning", "https://cdn.freesound.org/previews/696/696025_12711963-hq.mp3"),
    679737: ("Stasser", "Swallow male", "https://cdn.freesound.org/previews/679/679737_9581428-hq.mp3"),
    323003: ("asagatov", "Filling up a water bottle", "https://cdn.freesound.org/previews/323/323003_5113451-hq.mp3"),
    398757: ("Anthousai", "apple - bite & chew", "https://cdn.freesound.org/previews/398/398757_5923045-hq.mp3"),
    707808: ("stevielematt", "CHEWING_04_44100", "https://cdn.freesound.org/previews/707/707808_8476788-hq.mp3"),
}

# output (under Data/Audio) -> (source id, start s, end s, target loudness LUFS)
CUTS = {
    "voice/body_stomach_1": (447911, 0.00, 2.04, -24),
    "voice/body_stomach_2": (696025, 1.30, 3.18, -24),
    "voice/body_stomach_3": (696025, 3.62, 5.34, -24),
    "voice/body_dry_1": (679737, 0.62, 1.32, -24),
    "voice/body_dry_2": (679737, 2.68, 3.05, -24),
    "item/eat_bite_1": (398757, 0.00, 0.52, -18),
    "item/eat_bite_2": (398757, 1.56, 1.90, -18),
    "item/eat_chew_1": (707808, 0.00, 0.89, -21),
    "item/eat_chew_2": (707808, 1.03, 1.50, -21),
    "item/eat_chew_3": (707808, 1.60, 1.97, -21),
    "item/swallow_1": (679737, 0.62, 1.32, -21),
    "item/swallow_2": (679737, 2.68, 3.05, -21),
    "item/fill_1": (323003, 1.41, 4.20, -19),
    "item/fill_2": (323003, 5.00, 7.60, -19),
}


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


def build(work, audio):
    ff = os.environ.get("FFMPEG", "ffmpeg")
    for name, (sid, a, b, lufs) in CUTS.items():
        src = os.path.join(work, "dl", f"{sid}.mp3")
        dst = os.path.join(audio, name + ".ogg")
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        dur = b - a
        fade = min(0.03, dur / 6)
        chain = (f"atrim={a}:{b},asetpts=PTS-STARTPTS,highpass=f=60,"
                 f"afade=t=in:d={fade},afade=t=out:st={max(0.0, dur - fade)}:d={fade},"
                 f"loudnorm=I={lufs}:TP=-2:LRA=11,aresample=44100")
        subprocess.run([ff, "-hide_banner", "-loglevel", "error", "-y", "-i", src, "-af", chain, "-ac", "1",
                        "-c:a", "libvorbis", "-q:a", "4", dst], check=True)
        print(dst)


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "fetch":
        fetch(sys.argv[2])
    elif len(sys.argv) >= 4 and sys.argv[1] == "build":
        build(sys.argv[2], sys.argv[3])
    else:
        print(__doc__ or "fetch <work> | build <work> <Data/Audio>")
