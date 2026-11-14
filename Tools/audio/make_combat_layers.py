# The combat hit layers (COMBAT-AND-LESSONS L1: transient, body, material, voice; cooks/build-combat-core.md 3.2), derived
# with ffmpeg from the combat and world sounds already in Data/Audio (same CC0 / CC-BY sources, Docs/CREDITS.md):
#   hit_transient_<flesh|armor|stone>_N  the edge's crack: the first ~0.1 s of a hit, high-passed (2-6 kHz carries it)
#   hit_body_<light|heavy>_N             the mass's thud: a hit or a landing low-passed and pitched down (60-250 Hz)
#   stance_break_N                       a deep crack and give: armour and a landing pitched down together
#   guard_break_N                        the shield ringing out: a block pitched down over a bright armour hit
#   crit_stab_N                          a wet stab with weight under it
# Usage: python -I Tools/audio/make_combat_layers.py <Data/Audio>   (needs ffmpeg on PATH)
import os, subprocess, sys

AUDIO = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "Data", "Audio")
C = os.path.join(AUDIO, "combat")
W = os.path.join(AUDIO, "world")

def run(inputs, graph, out, seconds):
    args = ["ffmpeg", "-y", "-loglevel", "error"]
    for i in inputs:
        args += ["-i", i]
    args += ["-filter_complex", graph + ",alimiter=limit=0.708:attack=1:release=30:level=false,atrim=0:%.3f,afade=t=out:st=%.3f:d=%.3f,aresample=44100,aformat=channel_layouts=mono[o]"
             % (seconds, seconds * 0.55, seconds * 0.45), "-map", "[o]", "-c:a", "libvorbis", "-q:a", "4", out]
    subprocess.run(args, check=True)

def pitch(r):
    return "asetrate=44100*%.3f,aresample=44100" % r

def make():
    for n in (1, 2, 3):
        run([os.path.join(C, "hit_flesh_%d.ogg" % n)], "[0:a]highpass=f=1500,highpass=f=1500,volume=6.0", os.path.join(C, "hit_transient_flesh_%d.ogg" % n), 0.11)
        run([os.path.join(C, "hit_armor_%d.ogg" % n)], "[0:a]highpass=f=2200,volume=2.5", os.path.join(C, "hit_transient_armor_%d.ogg" % n), 0.14)
        run([os.path.join(W, "gather_stone_%d.ogg" % n)], "[0:a]highpass=f=1400,volume=3.5", os.path.join(C, "hit_transient_stone_%d.ogg" % n), 0.12)
        run([os.path.join(C, "hit_flesh_%d.ogg" % (n + 3))], "[0:a]" + pitch(0.85) + ",lowpass=f=260,lowpass=f=260,volume=3.2", os.path.join(C, "hit_body_light_%d.ogg" % n), 0.30)
        run([os.path.join(C, "land_%d.ogg" % n)], "[0:a]" + pitch(0.72) + ",lowpass=f=180,lowpass=f=180,volume=4.0", os.path.join(C, "hit_body_heavy_%d.ogg" % n), 0.45)
    for n in (1, 2):
        run([os.path.join(C, "hit_armor_%d.ogg" % (n + 3)), os.path.join(C, "land_%d.ogg" % (n + 2))],
            "[0:a]" + pitch(0.55) + ",volume=1.4[a];[1:a]" + pitch(0.6) + ",lowpass=f=300,volume=3.0[b];[a][b]amix=inputs=2:normalize=0",
            os.path.join(C, "stance_break_%d.ogg" % n), 0.95)
        run([os.path.join(C, "hit_block_%d.ogg" % n), os.path.join(C, "hit_armor_5.ogg")],
            "[0:a]" + pitch(0.82) + ",volume=1.3[a];[1:a]highpass=f=1800,volume=0.9[b];[a][b]amix=inputs=2:normalize=0",
            os.path.join(C, "guard_break_%d.ogg" % n), 0.8)
        run([os.path.join(C, "hit_flesh_%d.ogg" % (n + 5)), os.path.join(C, "land_%d.ogg" % n)],
            "[0:a]" + pitch(0.78) + ",volume=1.4[a];[1:a]" + pitch(0.7) + ",lowpass=f=200,volume=3.0[b];[a][b]amix=inputs=2:normalize=0",
            os.path.join(C, "crit_stab_%d.ogg" % n), 0.7)

if __name__ == "__main__":
    make()
    print("combat layers written to", C)
