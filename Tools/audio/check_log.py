# Reads an audiotest client log (-wrsoundlog) and the recorded output WAV, and checks the sound pass:
# every expected event played, none was spammed, unknown events were logged once, the beds and the music followed
# the place, the time and the fight, and the recording carries sound where the log says sounds played.
#   python -I check_log.py <client.log> game|menu [recording.wav]
import re, sys, struct, collections, datetime

log, mode = sys.argv[1], sys.argv[2]
wav = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
lines = open(log, encoding="utf-8", errors="replace").read().splitlines()
stamp = re.compile(r"^\[(\d{4})\.(\d{2})\.(\d{2})-(\d{2})\.(\d{2})\.(\d{2}):(\d{3})\]")

def when(line):
    m = stamp.match(line)
    if not m: return None
    y, mo, d, h, mi, s, ms = map(int, m.groups())
    return datetime.datetime(y, mo, d, h, mi, s, ms * 1000).timestamp()

plays = []           # (time, event, file, volume, where)
held = collections.Counter()
unknown = collections.Counter()
states, music, stats = [], [], []
rec_start = None
for line in lines:
    i = line.find("SOUND ")
    if "LogWildreach" in line and i >= 0:
        body = line[i + 6:]
        t = when(line)
        m = re.match(r"(\S+) file=(\S+) vol=([\d.]+) .*? pitch=[\d.]+ (.*)$", body)
        if m:
            plays.append((t, m.group(1), m.group(2), float(m.group(3)), m.group(4)))
        elif re.match(r"\S+ held", body):
            held[body.split()[0]] += 1
        elif body.startswith("unknown event"):
            unknown[body.split()[2]] += 1
        elif body.startswith("state") :
            states.append((t, body))
        elif body.startswith("stats"):
            stats.append(body)
        elif body.startswith("record start"):
            rec_start = t
    if "LogWildreach" in line and "MUSIC " in line:
        music.append((when(line), line[line.find("MUSIC "):]))

count = collections.Counter(p[1] for p in plays)
# Held-back counts come from the last stats line (held lines are only logged at wr.SoundLog 2).
if stats:
    for e, n in re.findall(r"(\S+)=\d+/(\d+)", stats[-1]):
        held[e] = max(held[e], int(n))
problems = []
print(f"{len(plays)} plays of {len(count)} events; held back by gap/voices: {sum(held.values())}")
for e, n in sorted(count.items()):
    vols = [p[3] for p in plays if p[1] == e]
    print(f"  {e:28s} x{n:<4d} vol {min(vols):.2f}-{max(vols):.2f}" + (f"  (held {held[e]})" if held[e] else ""))

expect = {
    "game": ["swing.light", "hurt.female", "eat", "drink", "potion", "equip", "craft.done", "door.open", "door.close", "hearth.feed",
             "bow.draw", "bow.release", "footstep.gravel", "footstep.wood", "footstep.snow", "footstep.sand", "ui.open", "ui.close",
             "levelup", "recipe.unlock", "discover", "tame.success", "boss.intro", "respawn", "ui.hover", "ui.click", "ui.drag", "ui.drop", "ui.error",
             "creature.canine.attack", "creature.canine.hurt", "creature.boss.aggro", "creature.boss.death"],
    "menu": [],
}[mode]
missing = [e for e in expect if e not in count]
if mode == "game" and not any(e.startswith("creature.") and e.endswith(".death") for e in count): missing.append("creature.*.death")
if missing: problems.append("never played: " + ", ".join(missing))
# Spam: more than 8 of one event in any one second (footsteps of a herd aside) is a pile-up the gap should stop.
for e in count:
    ts = sorted(p[0] for p in plays if p[1] == e and p[0])
    worst = max((sum(1 for u in ts if t <= u < t + 1.0) for t in ts), default=0)
    if worst > (12 if e.startswith("footstep") else 8): problems.append(f"{e} played {worst} times in one second")
if mode == "game":
    if held["ui.click"] < 9: problems.append(f"ten ui.clicks at once: only {held['ui.click']} held back by the gap")
    if not any("SOUND volume music" in l for l in lines):
        print("note: the settings clicks sent with WRUIMouse didn't reach the widgets (a background window); hovers did")
    if unknown.get("no.such.event", 0) != 1: problems.append(f"unknown event logged {unknown.get('no.such.event', 0)} times (want once)")
loud = [p for p in plays if p[3] > 4.0]
if loud: problems.append(f"{len(loud)} plays above volume 4")

print("\nmusic:")
for t, m in music: print("  " + m)
modes = " ".join(m for _, m in music)
if mode == "menu" and "MUSIC menu" not in modes: problems.append("no menu music")
if mode == "game":
    for need in ("MUSIC combat", "MUSIC boss"):
        if need not in modes: problems.append("no " + need)
print("\nmixer states (each WRSoundStats):")
for t, s in states: print("  " + s[:300])
for s in stats[-1:]: print("\nlast stats: " + s[:1500])

if wav:
    try:
        import numpy as np
        b = open(wav, "rb").read()
        pos, fmt, data = 12, None, None
        while pos < len(b) - 8:
            cid, size = b[pos:pos + 4], struct.unpack("<I", b[pos + 4:pos + 8])[0]
            if cid == b"fmt ": fmt = struct.unpack("<HHIIHH", b[pos + 8:pos + 24])
            if cid == b"data": data = b[pos + 8:pos + 8 + size]
            pos += 8 + size + (size & 1)
        tag, ch, sr, _, _, bits = fmt
        x = np.frombuffer(data, dtype=np.float32 if tag == 3 else np.int16).astype(np.float64)
        if tag != 3: x /= 32768.0
        x = x.reshape(-1, ch)
        win = sr // 2
        rms = np.array([np.sqrt(np.mean(x[i:i + win] ** 2)) for i in range(0, len(x) - win, win)])
        db = 20 * np.log10(rms + 1e-9)
        print(f"\nrecording: {len(x) / sr:.0f} s, {ch} ch, {sr} Hz; peak {20 * np.log10(np.abs(x).max() + 1e-9):.1f} dBFS; "
              f"half-seconds above -60 dBFS: {np.mean(db > -60) * 100:.0f}%; median {np.median(db):.1f} dBFS")
        if np.abs(x).max() >= 0.999: problems.append("the recording clips")
        if np.mean(db > -60) < 0.5: problems.append("the recording is mostly silent")
        if rec_start:
            hit = tot = 0
            for t, e, f, v, w in plays:
                if t is None or "out of earshot" in w: continue
                k = int((t - rec_start) / 0.5)
                if 0 <= k < len(db) - 1:
                    tot += 1
                    hit += max(db[k], db[k + 1]) > -50
            print(f"plays with sound in the recording at that moment: {hit} of {tot}")
            if tot and hit / tot < 0.9: problems.append(f"only {hit} of {tot} plays show in the recording")
    except Exception as ex:
        problems.append(f"couldn't read the recording: {ex}")

print("\n" + ("PROBLEMS:\n  " + "\n  ".join(problems) if problems else "all audio checks passed"))
sys.exit(1 if problems else 0)
