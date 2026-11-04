# Blender (headless): new animations for a creature rigged by rig_quadruped.py, onto its existing skeleton (no new
# mesh): feet that stay planted while they bear weight and step off the toe, a body that rises, rolls and sways with
# its legs, a steady head, a tail that trails behind on springs, and attacks with a wind-up, a fast strike and a
# recovery. Same six clips as before (Idle, Walk, Run, Attack, Hit, Death), written as an animations-only FBX for
# Tools/import_anims.py.
#   blender -b -P anim_creature.py -- <rigged.fbx> <out_clips.fbx> --id <creature id> [--build heavy|light]
#       [--attack bite|ram] [--hind-bend back|forward] [--motion procedural|barghest:<dir of exported FBXs>]
#       [--swing-foot follow|level] [--gait auto|hop] [--lift 1.0]
#       [--preview <dir>]
# Gaits come from the creature's size and speeds in Data/creatures.json (Froude number: walk, trot or amble, gallop),
# strides from the legs' reach, so the clips play near rate 1 in game and the feet don't slide. --gait hop: a hopper
# (Pumicehog) bounds at every speed, front paws landing together, then the hind feet. --lift 0.6: lower paw clearance
# in the swing (Rimeclaw's forearms swung up level like a show horse's at the light build's lift).
import bpy
import json
import math
import os
import sys

import numpy as np
from mathutils import Matrix, Quaternion, Vector

argv = sys.argv[sys.argv.index("--") + 1:]
src, out = argv[0], argv[1]


def opt(name, default=None):
    return argv[argv.index(name) + 1] if name in argv else default


cid = opt("--id")
build_name = opt("--build", "light")
attack_kind = opt("--attack", "bite")
hind_bend = opt("--hind-bend", "back")
motion = opt("--motion", "procedural")
swing_foot = opt("--swing-foot", "follow")  # follow (paws) | level (hooves)
gait_kind = opt("--gait", "auto")  # auto (from the Froude number) | hop
lift_k = float(opt("--lift", 1.0))  # scales the build's swing lift
preview = opt("--preview")
FPS = 60

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm = next(o for o in bpy.data.objects if o.type == "ARMATURE")
body = next((o for o in bpy.data.objects if o.type == "MESH"), None)
for a in list(bpy.data.actions):
    bpy.data.actions.remove(a)
scene = bpy.context.scene
scene.render.fps = FPS

# ---------------------------------------------------------------- the creature's numbers (Data/creatures.json)
data_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Data", "creatures.json")
with open(data_path, encoding="utf-8") as f:
    cdef = next(c for c in json.load(f)["creatures"] if c.get("id") == cid)  # (the "_bosses" divider has none)
# the mesh is 1 unit tall; Unreal fits it to the capsule, times meshScale (Cinderfang's raised tail)
HEIGHT_CM = cdef["halfHeight"] * 2.0 * cdef.get("meshScale", 1.0)
RUN_CMS = float(cdef["speed"])
WALK_CMS = RUN_CMS * 0.4  # the AI wanders at 0.4 input

# ---------------------------------------------------------------- the rig
bones = arm.data.bones
REST = {b.name: b.matrix_local.copy() for b in bones}
HEAD = {n: m.translation.copy() for n, m in REST.items()}
ROT = {n: m.to_quaternion() for n, m in REST.items()}
PARENT = {b.name: (b.parent.name if b.parent else None) for b in bones}
ORDER = []
while len(ORDER) < len(REST):
    for n in REST:
        if n not in ORDER and (PARENT[n] is None or PARENT[n] in ORDER):
            ORDER.append(n)
SPINE = [n for n in ("spine1", "spine2", "spine3") if n in REST]
TAIL = [n for n in ("tail1", "tail2") if n in REST]
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))


class Leg:
    def __init__(self, name):
        self.name, self.pair, self.side = name, name[0], (1 if name[1] == "L" else -1)
        self.hip0, self.knee0 = HEAD[name + "_upper"], HEAD[name + "_lower"]
        self.ankle0 = HEAD[name + "_foot"]
        self.a, self.b = (self.knee0 - self.hip0).length, (self.ankle0 - self.knee0).length
        fdir = REST[name + "_foot"].col[1].xyz.normalized()
        self.flen = self.ankle0.z / -fdir.z if fdir.z < -0.05 else bones[name + "_foot"].length
        self.fvec = fdir * self.flen  # ankle -> toe, resting on the ground
        self.toe0 = self.ankle0 + self.fvec
        self.toe0.z = 0.0
        self.parent = PARENT[name + "_upper"]
        # Front (and middle) legs fold at the wrist, apex forward; hind legs at the hock, apex back (pillar-legged
        # heavies bend both forward, like an elephant). A front joint high on the leg is an elbow (rig_quadruped
        # --front-elbow): apex back, and the paw stays in line with the forearm through the swing.
        self.elbow = self.pair != "R" and self.knee0.z > 0.5 * self.hip0.z
        self.bend = -1.0 if self.elbow else (1.0 if (self.pair != "R" or hind_bend == "forward") else -1.0)


LEGS = [Leg(n[:2]) for n in sorted(REST) if n.endswith("_upper") and n[:2] in ("FL", "FR", "ML", "MR", "RL", "RR")]
REAR = [l for l in LEGS if l.pair == "R"]
FRONT = [l for l in LEGS if l.pair == "F"]
HIP_H = sum(l.hip0.z for l in LEGS) / len(LEGS)
LEG_L = sum(l.a + l.b for l in LEGS) / len(LEGS)
REAR_X = sum(l.hip0.x for l in REAR) / len(REAR)
# two legs (rig_quadruped --legs 2: Magmaw, the birds): the body runs forward from the hips to the chest
FRONT_X = (sum(l.hip0.x for l in FRONT) / len(FRONT) if FRONT
           else max(HEAD[n].x for n in SPINE) if SPINE else REAR_X + 0.2)
BODY_LEN = FRONT_X - REAR_X
SPINE_Z = HEAD["pelvis"].z
MESH_LEN = (float(np.ptp(np.array([v.co.x for v in body.data.vertices]))) if body else BODY_LEN * 1.8)
L_REF = MESH_LEN  # how far its moves reach: nose to tail
# Stand square: image models come mid-stride (one front foot far ahead, the other back), and stepping around that
# stance runs the leading foot out of reach. Each pair's feet go side by side, mostly under their hips; front feet
# never behind the shoulders (a column, as in any standing animal: a front foot left behind slants the leg back as if
# falling forward).
for pair in "FMR":
    ls = [l for l in LEGS if l.pair == pair]
    if not ls:
        continue
    hx = sum(l.hip0.x for l in ls) / len(ls)
    ax = sum(l.ankle0.x for l in ls) / len(ls)
    ay = sum(abs(l.ankle0.y) for l in ls) / len(ls)
    for l in ls:
        x = hx + 0.35 * (ax - hx)
        ankle = Vector((max(x, hx) if pair == "F" else x, l.side * ay, l.ankle0.z))
        l.toe0 = ankle + l.fvec
        l.toe0.z = 0.0

# The torso's cross-section (for lying down when it dies): half-width and depth below the spine line, from the mesh.
TORSO_W, TORSO_D = 0.18, SPINE_Z * 0.4
if body:
    vs = np.array([body.matrix_world @ v.co for v in body.data.vertices])
    mid = vs[(vs[:, 0] > REAR_X + 0.2 * BODY_LEN) & (vs[:, 0] < FRONT_X - 0.2 * BODY_LEN) & (vs[:, 2] > HIP_H * 0.55)]
    if len(mid):
        TORSO_W = float(np.percentile(np.abs(mid[:, 1]), 95))
        TORSO_D = max(0.05, SPINE_Z - float(np.percentile(mid[:, 2], 3)))
print(f"RIG {cid}: {len(LEGS)} legs, hip {HIP_H:.3f} leg {LEG_L:.3f} body {BODY_LEN:.3f} torso w {TORSO_W:.3f} d {TORSO_D:.3f}")

# ---------------------------------------------------------------- builds: how a creature carries itself
BUILDS = {
    # nimble predators: springy, steady head, low tail carriage, big paw flick, gallops
    "light": dict(stand_f=0.99, stand_r=0.95, vault=0.012, bounce=0.035, roll=2.5, sway=0.006, yaw=3.5, lift=0.24, flick=75, toeoff=22,
                  steady=0.8, nod=2.0, tail_hz=2.2, tail_damp=0.35, flex=16, fast="gallop", breath=0.004),
    # heavy grazers: slow sway onto the planted side, a nodding head, shallow lift, ambles instead of galloping
    "heavy": dict(stand_f=0.993, stand_r=0.975, vault=0.010, bounce=0.022, roll=4.5, sway=0.022, yaw=2.0, lift=0.13, flick=40, toeoff=15,
                  steady=0.55, nod=5.0, tail_hz=2.8, tail_damp=0.45, flex=6, fast="amble", breath=0.006),
}
B = BUILDS[build_name]
R = math.radians


def stand_height(pair, ext):
    """Girdle offset from rest that stands this pair's legs at `ext` of their full length over their feet."""
    ls = [l for l in LEGS if l.pair == pair]
    dz = []
    for l in ls:
        ankle = l.toe0 - l.fvec
        dh = math.hypot(ankle.x - l.hip0.x, ankle.y - l.hip0.y)
        reach = ext * (l.a + l.b)
        dz.append(ankle.z + math.sqrt(max(0.0, reach * reach - dh * dh)) - l.hip0.z)
    return sum(dz) / len(dz) if dz else 0.0


# How high each girdle stands. Front legs nearly straight: the bend sits halfway down the forearm, where the real
# animal has no joint, so a crouched front leg reads as an extra joint. Hind legs stand angled at the hock.
STAND_R = stand_height("R", B["stand_r"])
STAND_F = stand_height("F", B["stand_f"]) if FRONT else STAND_R
# ...but the back stays near level: hind legs that could stand taller than the front crouch a little more instead.
STAND_R = min(STAND_R, STAND_F + BODY_LEN * math.tan(R(2)))
print(f"STAND rear {STAND_R:+.3f} front {STAND_F:+.3f} (pitch {math.degrees(math.atan2(STAND_F - STAND_R, BODY_LEN)):+.1f} deg)")


def q_eul(pitch_up=0.0, roll=0.0, yaw=0.0):
    """World-axis rotation: pitch nose-up, roll left side up, yaw nose left (radians)."""
    return Quaternion(Z, yaw) @ Quaternion(Y, -pitch_up) @ Quaternion(X, roll)


def smooth(x):
    x = min(1.0, max(0.0, x))
    return x * x * (3 - 2 * x)


def minjerk(x):
    x = min(1.0, max(0.0, x))
    return x * x * x * (10 - 15 * x + 6 * x * x)


EASE = {"io": smooth, "lin": lambda x: x, "out": lambda x: 1 - (1 - x) ** 3, "in": lambda x: x ** 3,
        "snap": lambda x: 1 - (1 - x) ** 5, "mj": minjerk}


def keys(t, pts, ease="io"):
    """Piecewise curve through (time, value[, ease]) points; holds the ends."""
    if t <= pts[0][0]:
        return pts[0][1]
    for i in range(len(pts) - 1):
        t0, v0 = pts[i][0], pts[i][1]
        t1, v1 = pts[i + 1][0], pts[i + 1][1]
        if t <= t1:
            e = EASE[pts[i + 1][2] if len(pts[i + 1]) > 2 else ease]((t - t0) / max(1e-6, t1 - t0))
            return v0 + (v1 - v0) * e
    return pts[-1][1]


def settle(t, t0, amp, hz, damp):
    """A knock that rings down: amp at t0, damped oscillation after."""
    if t < t0:
        return 0.0
    w = 2 * math.pi * hz
    return amp * math.exp(-damp * w * (t - t0)) * math.cos(w * math.sqrt(max(0.0, 1 - damp * damp)) * (t - t0))


class State:
    """One frame of a clip, before solving: where the body is and where each foot goes."""

    def __init__(self):
        self.off = Vector()  # pelvis offset
        self.pitch = self.roll = self.yaw = 0.0  # pelvis (and so the whole body)
        self.zf = 0.0  # front girdle height offset (pitch from it unless set)
        self.flex = 0.0  # spine arch (+ gathered, - stretched)
        self.spine_roll = self.spine_yaw = 0.0  # chest vs pelvis
        self.neck = (0.0, 0.0, 0.0)  # explicit neck / head local rotations (pitch, roll, yaw)
        self.head = (0.0, 0.0, 0.0)
        self.steady = 0.0  # how much the head holds its world orientation
        self.look = (0.0, 0.0)  # head world pitch / yaw it holds (from rest)
        self.tail = (0.0, 0.0)  # tail base pitch / yaw (local, before springs)
        self.feet = {}  # leg name -> (ankle Vector, toe-down radians)
        self.follow = {}  # leg name -> 0..1: the paw keeps its rest angle to the lower leg instead of the toe-down angle
        self.limp = 0.0  # 0 legs on IK, 1 legs hanging limp (dying)
        self.lying = False
        # world rotations from rest taken straight from a source clip (retargeting); None = built from the numbers above
        self.q_pelvis = self.q_chest = self.q_neck = self.q_head = None
        self.q_tail = None


def foot_dir(leg, alpha):
    return Quaternion(Y, alpha) @ leg.fvec  # toe down about the lateral axis


def planted(leg, dx=0.0, alpha=0.0, lift=0.0):
    """Ankle for the foot resting at its rest spot moved dx forward, pivoting on the toe by alpha."""
    toe = leg.toe0 + Vector((dx, 0, 0))
    a = toe - foot_dir(leg, alpha)
    a.z += lift
    return a, alpha


# ---------------------------------------------------------------- solving a frame: FK body, IK legs
def frame_rot(ydir, zhint):
    yv = ydir.normalized()
    zv = (zhint - yv * zhint.dot(yv)).normalized()
    xv = yv.cross(zv)
    m = Matrix((xv, yv, zv)).transposed()
    return m.to_quaternion()


def solve(st):
    """Posed armature-space matrices for every bone."""
    H, D = {}, {}  # posed head positions, world rotation deltas from rest
    H["root"], D["root"] = HEAD["root"].copy(), Quaternion()
    pitch = st.pitch + math.atan2(st.zf - st.off.z, BODY_LEN) + st.flex / 2
    D["pelvis"] = q_eul(math.atan2(st.zf - st.off.z, BODY_LEN)) @ st.q_pelvis if st.q_pelvis else q_eul(pitch, st.roll, st.yaw)
    H["pelvis"] = HEAD["pelvis"] + st.off

    def child(n, L):
        p = PARENT[n]
        H[n] = H[p] + D[p] @ (HEAD[n] - HEAD[p])
        D[n] = D[p] @ L

    def child_to(n, world):
        """Child whose world rotation from rest is given."""
        child(n, Quaternion())
        D[n] = world

    if st.q_chest:
        bend = D["pelvis"].inverted() @ st.q_chest  # spread the chest's turn over the spine
        for n in SPINE:
            child(n, Quaternion().slerp(bend, 1.0 / len(SPINE)))
    else:
        for n in SPINE:
            child(n, q_eul(-st.flex / len(SPINE), st.spine_roll / len(SPINE), st.spine_yaw / len(SPINE)))
    # neck and head: explicit pose, then pulled toward a steady world orientation (the neck takes most of it)
    if st.q_neck:
        child_to("neck", st.q_neck)
        child_to("head", st.q_head)
    else:
        child("neck", q_eul(*st.neck))
        child("head", q_eul(*st.head))
    if st.steady > 0:
        want = D["head"].slerp(q_eul(st.look[0], 0.0, st.look[1]), st.steady)
        corr = want @ D["head"].inverted()
        D["neck"] = Quaternion().slerp(corr, 0.55) @ D["neck"]
        H["head"] = H["neck"] + D["neck"] @ (HEAD["head"] - HEAD["neck"])
        D["head"] = want
    for i, n in enumerate(TAIL):
        if st.q_tail:
            child_to(n, st.q_tail[i])
        else:
            child(n, q_eul(st.tail[0] * (1 if i == 0 else 0.6), 0.0, st.tail[1] * (1 if i == 0 else 0.6)))

    P = {}
    for n in H:
        P[n] = Matrix.LocRotScale(H[n], D[n] @ ROT[n], None)
    for leg in LEGS:
        p = leg.parent
        hip = H[p] + D[p] @ (leg.hip0 - HEAD[p])
        lat = (D[p] @ Y).normalized()
        if st.limp >= 1.0 or leg.name not in st.feet:
            ik = None
        else:
            ankle, alpha = st.feet[leg.name]
            fwd = (D[p] @ X).normalized()
            ik = ik_leg(leg, hip, ankle, alpha, fwd * leg.bend, lat, st.follow.get(leg.name, 0.0))
        limp = limp_leg(leg, hip, D[p]) if st.limp > 0 else None
        if ik and limp:
            mats = [Matrix.LocRotScale(None, a.to_quaternion().slerp(b.to_quaternion(), smooth(st.limp)), None) for a, b in zip(ik, limp)]
            mats = fk_leg(leg, hip, [m.to_quaternion() for m in mats])
        else:
            mats = ik or limp
        if not mats:
            continue  # body only (measuring reach)
        for part, m in zip(("_upper", "_lower", "_foot"), mats):
            P[leg.name + part] = m
    return P, H, D


def ik_leg(leg, hip, ankle, alpha, pole, lat, follow=0.0):
    a, b = leg.a, leg.b
    v = ankle - hip
    d = min(max(v.length, abs(a - b) + 1e-4), (a + b) * 0.9995)
    u = v.normalized()
    w = pole - u * pole.dot(u)
    w = w.normalized() if w.length > 1e-6 else Z.cross(u).normalized()
    c = (a * a + d * d - b * b) / (2 * a * d)
    th = math.acos(min(1.0, max(-1.0, c)))
    knee = hip + (u * math.cos(th) + w * math.sin(th)) * a
    end = hip + u * d
    n = u.cross(w).normalized()
    if n.dot(lat) < 0:
        n = -n
    fd = foot_dir(leg, alpha)
    if follow > 0.0:
        turn = (leg.ankle0 - leg.knee0).normalized().rotation_difference((end - knee).normalized())
        fd = fd.normalized().lerp((turn @ leg.fvec).normalized(), follow)
    return [Matrix.LocRotScale(hip, frame_rot(knee - hip, n), None),
            Matrix.LocRotScale(knee, frame_rot(end - knee, n), None),
            Matrix.LocRotScale(end, frame_rot(fd, n), None)]


def fk_leg(leg, hip, rots):
    """Leg matrices from three world rotations, keeping bone lengths."""
    up = rots[0] @ Y
    knee = hip + up * leg.a
    lo = rots[1] @ Y
    end = knee + lo * leg.b
    return [Matrix.LocRotScale(hip, rots[0], None), Matrix.LocRotScale(knee, rots[1], None),
            Matrix.LocRotScale(end, rots[2], None)]


def limp_leg(leg, hip, Dp):
    """A dead leg: the rest pose carried by the body, slightly folded."""
    fold = Quaternion(Y, R(-14) * leg.bend * -1)
    r_up = Dp @ fold @ ROT[leg.name + "_upper"]
    r_lo = Dp @ fold @ Quaternion(Y, R(22) * leg.bend * -1) @ ROT[leg.name + "_lower"]
    r_ft = Dp @ fold @ Quaternion(Y, R(30)) @ ROT[leg.name + "_foot"]
    return fk_leg(leg, hip, [r_up, r_lo, r_ft])


def leg_drop(st):
    """How far each girdle must come down for every planted foot to be reachable (rear, front)."""
    _, H, D = solve_body_only(st)
    need = {"R": 0.0, "F": 0.0}
    for leg in LEGS:
        if leg.name not in st.feet:
            continue
        p = leg.parent
        hip = H[p] + D[p] @ (leg.hip0 - HEAD[p])
        ankle = st.feet[leg.name][0]
        reach = (leg.a + leg.b) * 0.985
        dh = (Vector((ankle.x, ankle.y, 0)) - Vector((hip.x, hip.y, 0))).length
        dz_max = math.sqrt(max(0.0, reach * reach - dh * dh))
        drop = max(0.0, (hip.z - ankle.z) - dz_max)
        if leg.pair == "M":
            w = (leg.hip0.x - REAR_X) / max(1e-6, BODY_LEN)
            need["R"] = max(need["R"], drop * (1 - w) * 1.2)
            need["F"] = max(need["F"], drop * w * 1.2)
        else:
            need[leg.pair if leg.pair in need else "R"] = max(need[leg.pair if leg.pair in need else "R"], drop)
    return need["R"], need["F"]


def solve_body_only(st):
    keep = st.feet
    st.feet = {}
    r = solve(st)
    st.feet = keep
    return r


def fit_reach(states, loop):
    """Lower each girdle where a planted foot couldn't reach, smoothed so it reads as weight, not a glitch."""
    n = len(states)
    for _ in range(3):
        dr = np.array([0.0] * n)
        df = np.array([0.0] * n)
        for i, st in enumerate(states):
            if st.limp >= 1.0:
                continue
            dr[i], df[i] = leg_drop(st)
            if not FRONT:  # two legs: the whole body comes down, or it tips back over its hips (Magmaw's bite reared)
                df[i] = dr[i]
        if dr.max() < 1e-4 and df.max() < 1e-4:
            break
        for arr in (dr, df):
            grown = np.array([arr[[(j + k) % n if loop else min(n - 1, max(0, j + k)) for k in range(-3, 4)]].max() for j in range(n)])
            arr[:] = blur(grown, 2.0, loop)
        for i, st in enumerate(states):
            st.off.z -= dr[i] * 1.05
            st.zf -= df[i] * 1.05


def blur(arr, sigma, loop):
    n = len(arr)
    k = np.exp(-0.5 * (np.arange(-6, 7) / sigma) ** 2)
    k /= k.sum()
    outv = np.zeros(n)
    for j in range(n):
        idx = [(j + o) % n if loop else min(n - 1, max(0, j + o)) for o in range(-6, 7)]
        outv[j] = float((arr[idx] * k).sum())
    return outv


# ---------------------------------------------------------------- springs: the tail trails the body
def tail_springs(poses, loop, hz, damp):
    """Swing each tail bone toward where the body carries it, on a damped spring (posed matrices edited in place)."""
    if not TAIL:
        return
    n = len(poses)
    dt = 1.0 / FPS
    w = 2 * math.pi * hz
    reps = 4 if loop else 1
    tips = {}
    for name in TAIL:
        ln = bones[name].length
        tips[name] = [P[name] @ Vector((0, ln, 0)) for P in poses]
    sim = {name: [None] * n for name in TAIL}
    for name in TAIL:
        pos = tips[name][0].copy()
        vel = Vector()
        for r in range(reps):
            for i in range(n - (1 if loop else 0)):
                target = tips[name][i]
                acc = (target - pos) * (w * w) - vel * (2 * damp * w)
                vel += acc * dt
                pos += vel * dt
                if r == reps - 1:
                    sim[name][i] = pos.copy()
        if loop:
            sim[name][n - 1] = sim[name][0]
    # re-aim the chain at the simulated tips, root first; children follow their parent's swing
    for i, P in enumerate(poses):
        carry = Quaternion()
        for name in TAIL:
            m = P[name]
            head = m.translation
            rigid_dir = (carry @ (m.to_quaternion() @ Y)).normalized()
            if name != TAIL[0]:
                prev = P[TAIL[TAIL.index(name) - 1]]
                head = prev @ Vector((0, bones[TAIL[TAIL.index(name) - 1]].length, 0))
            want = (sim[name][i] - head)
            if want.length < 1e-6:
                continue
            swing = rigid_dir.rotation_difference(want.normalized())
            ang = swing.angle
            if ang > R(50):
                swing = Quaternion().slerp(swing, R(50) / ang)
            rot = swing @ carry @ m.to_quaternion()
            P[name] = Matrix.LocRotScale(head, rot, None)
            carry = swing @ carry


# ---------------------------------------------------------------- writing actions
def write_action(name, poses, loop):
    act = bpy.data.actions.new(name)
    arm.animation_data_create()
    arm.animation_data.action = act
    prev = {}
    for f, P in enumerate(poses):
        for n in ORDER:
            pb = arm.pose.bones[n]
            pb.rotation_mode = "QUATERNION"
            par = PARENT[n]
            Pn = P.get(n) or (P[par] @ REST[par].inverted() @ REST[n] if par else REST[n])
            P[n] = Pn
            basis = (REST[n].inverted() @ Pn) if par is None else (P[par] @ REST[par].inverted() @ REST[n]).inverted() @ Pn
            loc, q, _ = basis.decompose()
            if n in prev and prev[n].dot(q) < 0:
                q = -q
            prev[n] = q
            pb.rotation_quaternion = q
            pb.location = loc if n in ("root", "pelvis") else Vector()
            pb.keyframe_insert("rotation_quaternion", frame=f + 1)
            if n in ("root", "pelvis"):
                pb.keyframe_insert("location", frame=f + 1)
    act.use_fake_user = True
    # the keys must reproduce the solved poses: check a few frames
    worst = 0.0
    for f in range(0, len(poses), max(1, len(poses) // 5)):
        scene.frame_set(f + 1)
        bpy.context.view_layer.update()
        for n in ORDER:
            worst = max(worst, (arm.pose.bones[n].matrix.translation - poses[f][n].translation).length,
                        (arm.pose.bones[n].matrix.col[1].xyz - poses[f][n].col[1].xyz).length)
    print(f"ACTION {name}: {len(poses)} frames ({(len(poses) - 1) / FPS:.2f} s){' loop' if loop else ''}, key error {worst:.4f}")
    return act


def bake(name, states, loop, tail=True):
    fit_reach(states, loop)
    poses = [solve(st)[0] for st in states]
    if os.environ.get("WR_TRACE") == name:  # where the pelvis and the head go, every 0.1 s
        for f in range(0, len(poses), 6):
            print(f"TRACE {name} {f / FPS:.2f}s pelvis {tuple(round(c, 3) for c in poses[f]['pelvis'].translation)}"
                  f" head {tuple(round(c, 3) for c in (poses[f]['head'] @ Vector((0, bones['head'].length, 0))))}")
    if tail:
        tail_springs(poses, loop, B["tail_hz"], B["tail_damp"])
    return write_action(name, poses, loop)


# ---------------------------------------------------------------- locomotion
GAITS = {
    # duty factor, footfall phase by leg (touchdown, fraction of the stride), for four and six legs
    "walk": (0.68, {"RL": 0.0, "FL": 0.25, "RR": 0.5, "FR": 0.75},
             {"RL": 0.0, "ML": 0.2, "FL": 0.4, "RR": 0.5, "MR": 0.7, "FR": 0.9}),
    "amble": (0.5, {"RL": 0.0, "FL": 0.25, "RR": 0.5, "FR": 0.75},
              {"RL": 0.0, "ML": 0.2, "FL": 0.4, "RR": 0.5, "MR": 0.7, "FR": 0.9}),
    "trot": (0.42, {"RL": 0.0, "FR": 0.0, "RR": 0.5, "FL": 0.5},
             {"RL": 0.0, "MR": 0.0, "FL": 0.0, "RR": 0.5, "ML": 0.5, "FR": 0.5}),
    "gallop": (0.32, {"RR": 0.0, "RL": 0.08, "FR": 0.42, "FL": 0.5},
               {"RR": 0.0, "RL": 0.08, "MR": 0.21, "ML": 0.29, "FR": 0.42, "FL": 0.5}),
    # a hop (half bound): the front paws land almost together, the hind feet land together half a stride later, and
    # the body floats between (gathered after the front lift off, stretched after the hind push off)
    "hop": (0.42, {"FL": 0.0, "FR": 0.04, "RL": 0.5, "RR": 0.53},
            {"FL": 0.0, "FR": 0.04, "ML": 0.25, "MR": 0.29, "RL": 0.5, "RR": 0.53}),
}


def pick_gait(v_cms):
    h_m = HIP_H * HEIGHT_CM / 100.0
    fr = (v_cms / 100.0) ** 2 / (9.81 * h_m)
    if gait_kind == "hop" and FRONT:
        return "hop", fr
    if fr < 0.35:
        g = "walk"
    elif fr < (2.5 if B["fast"] == "amble" else 2.0):
        g = "amble" if B["fast"] == "amble" else "trot"  # heavies amble; a charging rhino still gallops
    else:
        g = "gallop"
    if len(LEGS) == 6 and g == "gallop":
        g = "trot"  # six legs scurry in tripods at every speed (a galloping beetle bounds like a dog)
    return g, fr


def locomotion(name, v_cms):
    gait, fr = pick_gait(v_cms)
    D, ph4, ph6 = GAITS[gait]
    if build_name == "heavy" and gait == "walk":
        D = 0.72
    if gait == "hop" and v_cms > WALK_CMS * 1.5:
        D = 0.32  # running hops: longer floats
    phase = ph6 if len(LEGS) == 6 else ph4
    if not FRONT:  # two legs step in turn, whatever the gait
        phase = {"RL": 0.0, "RR": 0.5}
    v = v_cms / HEIGHT_CM  # units / s
    lam_alex = 2.3 * fr ** 0.3 * HIP_H  # Alexander's stride for this Froude number
    reach_k = {"walk": 0.82, "amble": 0.9, "trot": 0.92, "gallop": 1.15, "hop": 1.0}[gait]
    lam = min(lam_alex, reach_k * LEG_L / D)
    if len(LEGS) == 6:
        lam = min(lam, 1.2 * LEG_L)  # insects patter: short quick steps (long ones reach out like arms)
    T = lam / v
    N = max(8 if len(LEGS) == 6 else 12, round(T * FPS))
    T = N / FPS
    S = v * D * T  # how far a planted foot travels back
    lift = lift_k * B["lift"] * LEG_L * {"walk": 1.0, "amble": 1.15, "trot": 1.35, "gallop": 1.6, "hop": 1.2 if D > 0.4 else 1.6}[gait]
    run = gait != "walk"
    print(f"GAIT {name}: {gait} (Froude {fr:.2f}) {v_cms:.0f} cm/s, stride {lam:.3f} u ({lam_alex:.3f} free), {T:.2f} s, duty {D}")
    toeoff, flick = R(B["toeoff"]), R(B["flick"])
    states = []
    for f in range(N + 1):
        t = f / FPS
        st = State()
        st.steady, st.look = B["steady"] + (0.1 if run else 0.0), (0.0, 0.0)
        bumps = {}
        swings = {}
        xs = {}
        for leg in LEGS:
            p = (t / T - phase[leg.name]) % 1.0
            flick_l = 0.0 if leg.elbow else flick * (1.0 if leg.pair != "R" else 0.65)
            if p < D:
                s = p / D
                dx = S / 2 - S * s
                a = toeoff * smooth((s - 0.8) / 0.2)
                st.feet[leg.name] = planted(leg, dx, a)
                bumps[leg.name], swings[leg.name] = math.sin(math.pi * s), 0.0
            else:
                s = (p - D) / (1 - D)
                a0, _ = planted(leg, -S / 2, toeoff)
                a1, _ = planted(leg, S / 2, 0.0)
                e = minjerk(s)
                ankle = a0.lerp(a1, e)
                ankle.z += lift * math.sin(math.pi * s ** 0.8)
                alpha = toeoff * (1 - s) ** 2 + flick_l * math.sin(math.pi * s) ** 1.2 * (1 - s) ** 0.8
                toe = ankle + foot_dir(leg, alpha)
                if toe.z < 0.003:
                    ankle.z += 0.003 - toe.z
                st.feet[leg.name] = (ankle, alpha)
                # the paw swings in line with the forearm, flat again for touching down; hooves keep hanging at their
                # standing angle instead (in line with a forward-swinging leg they point straight ahead: Emberhorn)
                if leg.elbow and swing_foot == "follow":
                    st.follow[leg.name] = smooth(s / 0.25) * smooth((1 - s) / 0.25)
                bumps[leg.name], swings[leg.name] = 0.0, math.sin(math.pi * s)
            xs[leg.name] = st.feet[leg.name][0].x - leg.ankle0.x
        states.append((st, bumps, swings, xs))

    # body: heights from the planted legs (vaulting over them walking, bouncing on them running), roll toward the
    # lifted leg, sway onto the planted side, girdles turning with the legs, the spine arching in a gallop
    n = len(states)
    zr, zf, roll_r, roll_f, yaw_r, yaw_f, sway = (np.zeros(n) for _ in range(7))
    sgn = 1.0 if not run else -1.0
    amp = B["vault"] if not run else B["bounce"] * (1.4 if gait in ("gallop", "hop") else 1.0)
    for i, (st, bumps, swings, xs) in enumerate(states):
        def girdle(pair):
            ls = [l for l in LEGS if l.pair == pair]
            return sum(bumps[l.name] for l in ls) / len(ls) if ls else 0.0

        zr[i] = sgn * amp * HIP_H * girdle("R")
        zf[i] = sgn * amp * HIP_H * girdle("F")

        def side(pair, key, vals):
            ls = [l for l in LEGS if l.pair == pair]
            lv = sum(vals[l.name] for l in ls if l.side > 0)
            rv = sum(vals[l.name] for l in ls if l.side < 0)
            return lv - rv

        roll_r[i] = -R(B["roll"]) * side("R", "s", swings)
        roll_f[i] = -R(B["roll"]) * side("F", "s", swings)
        yaw_r[i] = -R(B["yaw"]) * side("R", "x", xs) / max(1e-6, S)
        yaw_f[i] = -R(B["yaw"]) * side("F", "x", xs) / max(1e-6, S)
        down_l = sum(1 for l in LEGS if l.side > 0 and bumps[l.name] > 0)
        down_r = sum(1 for l in LEGS if l.side < 0 and bumps[l.name] > 0)
        sway[i] = B["sway"] * HIP_H * (down_l - down_r) / (len(LEGS) / 2)
    for arr in (zr, zf):
        arr -= arr.mean()
    lead = max(1, round(0.08 * N))  # weight shifts before the leg lifts
    sway = blur(np.roll(sway, -lead), 3.0, True)
    roll_r, roll_f = blur(roll_r, 1.5, True), blur(roll_f, 1.5, True)
    t_hind = min(phase[l.name] for l in REAR)
    for i, (st, bumps, swings, xs) in enumerate(states):
        t = i / FPS
        st.off = Vector((0.0, sway[i], STAND_R + zr[i]))
        st.zf = STAND_F + zf[i]
        st.roll, st.yaw = roll_r[i], yaw_r[i]
        st.spine_roll, st.spine_yaw = roll_f[i] - roll_r[i], yaw_f[i] - yaw_r[i]
        st.off.y -= BODY_LEN / 2 * math.sin(st.yaw)  # turn about the middle, not the hips
        if gait in ("gallop", "hop") and FRONT:
            st.flex = R(B["flex"]) * math.cos(2 * math.pi * (t / T - t_hind))
        lead_legs = FRONT or REAR
        nod = R(B["nod"]) * (sum(bumps[l.name] for l in lead_legs) / len(lead_legs) - 0.5) * (-1 if not run else 1)
        st.look = (nod, 0.0)
        st.tail = (R(-8) if run else 0.0, 0.0)
    return bake(name, [s[0] for s in states], loop=True)


# ---------------------------------------------------------------- idle: breathing, weight shifts, looking around
def idle():
    L = 8.0
    N = round(L * FPS)
    states = []
    for f in range(N + 1):
        t = f / FPS
        st = State()
        br = math.sin(2 * math.pi * t * 3 / L) * 0.5 + 0.5  # three breaths
        br = br ** 1.4
        shift = keys(t, [(0, 0), (0.6, 0), (1.6, 1), (4.2, 1), (5.4, -0.8), (7.0, -0.8), (8.0, 0)])
        st.off = Vector((keys(t, [(0, 0), (1.6, -0.006), (4.2, -0.006), (5.4, 0.004), (7, 0.004), (8, 0)]) * HIP_H,
                         shift * B["sway"] * HIP_H * 0.7, STAND_R + B["breath"] * HIP_H * br))
        st.zf = STAND_F + B["breath"] * HIP_H * br * 1.6
        st.roll = R(1.2) * shift
        st.flex = R(1.5) * br
        yaw = keys(t, [(0, 0), (1.2, 0), (1.8, 26), (3.0, 26), (3.5, 0), (4.8, 0), (5.4, -18), (6.5, -18), (7.2, 0), (8, 0)])
        pit = keys(t, [(0, 0), (1.2, 0), (1.8, -6), (3.0, -6), (3.5, 0), (4.8, 0), (5.4, 9), (5.7, 7), (6.0, 10), (6.5, 9), (7.2, 0), (8, 0)])
        st.steady, st.look = 0.75, (R(pit), R(yaw))
        st.neck = (0.0, 0.0, R(yaw) * 0.3)
        swish = settle(t, 2.4, 1.0, 1.6, 0.25) + settle(t, 6.1, -0.8, 1.6, 0.25)
        st.tail = (R(-2) * br, R(14) * swish)
        for leg in LEGS:
            st.feet[leg.name] = planted(leg)
        states.append(st)
    return bake("Idle", states, loop=True)


# ---------------------------------------------------------------- attack: wind-up, strike, recovery
def attack():
    states = []
    if attack_kind == "ram":
        L, strike = 1.45, 0.68
    else:
        L, strike = 1.25, 0.57
    N = round(L * FPS)
    step_leg = next((l for l in (FRONT or REAR) if l.side > 0), (FRONT or REAR)[0])
    for f in range(N + 1):
        t = f / FPS
        st = State()
        if attack_kind == "ram":
            # head drops and the weight rocks back onto the hind legs; a surge forward, the head tossed up through
            # the target; hold; settle back
            x = keys(t, [(0, 0), (0.55, -0.06), (strike, 0.12, "snap"), (0.85, 0.1), (1.45, 0, "io")]) * L_REF
            zr = keys(t, [(0, 0), (0.55, -0.035), (strike, 0.01, "snap"), (0.85, 0.01), (1.45, 0)]) * HIP_H
            zf = keys(t, [(0, 0), (0.55, -0.06), (strike, 0.04, "snap"), (0.85, 0.03), (1.45, 0)]) * HIP_H
            neck = keys(t, [(0, 0), (0.55, -28), (strike - 0.04, -20, "out"), (strike + 0.06, 22, "snap"), (0.9, 18), (1.45, 0)])
            head = keys(t, [(0, 0), (0.55, -18), (strike - 0.04, -10, "out"), (strike + 0.06, 20, "snap"), (0.9, 14), (1.45, 0)])
            st.neck, st.head = (R(neck), 0.0, 0.0), (R(head), 0.0, R(settle(t, strike + 0.06, 10, 3.0, 0.4)))
            st.tail = (R(keys(t, [(0, 0), (0.55, 12), (strike, -6), (1.45, 0)])), 0.0)
        else:
            # coil back and down, weight over the hind legs, head drawn back; snap forward in a tenth of a second,
            # the front foot stepping in; a shake of the head on the bite; ease back
            x = keys(t, [(0, 0), (0.45, -0.06), (strike, 0.2, "snap"), (0.72, 0.17), (1.25, 0, "io")]) * L_REF
            if not FRONT:  # no front legs to reach out on: a shorter lunge over the planted feet
                x *= 0.5
            zr = keys(t, [(0, 0), (0.45, -0.045), (strike, -0.01, "snap"), (0.72, -0.01), (1.25, 0)]) * HIP_H
            zf = keys(t, [(0, 0), (0.45, -0.07), (strike, 0.02, "snap"), (0.72, 0.0), (1.25, 0)]) * HIP_H
            neck = keys(t, [(0, 0), (0.45, 18), (strike, -20, "snap"), (0.72, -12), (1.25, 0)])
            head = keys(t, [(0, 0), (0.45, 22), (strike - 0.03, -20, "snap"), (strike + 0.05, 8, "out"), (0.72, 0), (1.25, 0)])
            shake = settle(t, strike + 0.02, 12, 6.0, 0.25) if t < 0.9 else 0.0
            st.neck, st.head = (R(neck), 0.0, R(shake * 0.4)), (R(head), R(shake * 0.5), R(shake))
            st.tail = (R(keys(t, [(0, 0), (0.45, -10), (strike, 6), (1.25, 0)])), 0.0)
        st.off = Vector((x, 0.0, STAND_R + zr))
        st.zf = STAND_F + zf
        st.steady = 0.0
        for leg in LEGS:
            if leg is step_leg:
                dx = keys(t, [(0, 0), (strike - 0.1, 0), (strike + 0.04, 0.16 * L_REF, "out"), (0.8, 0.16 * L_REF),
                              (1.05, 0, "io")])
                up = 0.0
                if strike - 0.1 < t < strike + 0.04:
                    up = math.sin(math.pi * (t - strike + 0.1) / 0.14) * 0.12 * LEG_L
                elif 0.8 < t < 1.05:
                    up = math.sin(math.pi * (t - 0.8) / 0.25) * 0.08 * LEG_L
                st.feet[leg.name] = planted(leg, dx, lift=up)
            else:
                st.feet[leg.name] = planted(leg)
        states.append(st)
    return bake("Attack", states, loop=False)


# ---------------------------------------------------------------- hit: a jolt that rings down
def hit():
    L = 0.6
    N = round(L * FPS)
    states = []
    for f in range(N + 1):
        t = f / FPS
        st = State()
        k = settle(t, 0.03, 1.0, 2.6, 0.45) * smooth(t / 0.03) * smooth((L - t) / 0.12)
        st.off = Vector((-0.035 * L_REF * k, 0.01 * k, STAND_R - 0.02 * HIP_H * abs(k)))
        st.zf = STAND_F + 0.03 * HIP_H * k
        st.roll = R(5) * k
        st.neck = (R(14) * k, 0.0, R(-10) * k)
        st.head = (R(10) * k, R(6) * k, R(-8) * k)
        st.tail = (R(10) * k, R(-12) * k)
        for leg in LEGS:
            st.feet[leg.name] = planted(leg)
        states.append(st)
    return bake("Hit", states, loop=False)


# ---------------------------------------------------------------- death: stagger, legs buckle, rolls onto its side
def death():
    L = 2.0
    N = round(L * FPS)
    states = []
    lie_z = math.sqrt(TORSO_W ** 2)  # spine line resting on the torso's side
    for f in range(N + 1):
        t = f / FPS
        st = State()
        # front girdle buckles first, then the rear; then the body tips over to its left side and settles
        zf = keys(t, [(0, 0), (0.15, 0.02), (0.55, -0.45, "in"), (0.75, -0.5)]) * HIP_H
        zr = keys(t, [(0, 0), (0.35, 0.0), (0.8, -0.42, "in"), (0.95, -0.46)]) * HIP_H
        roll = keys(t, [(0, 0), (0.25, -6), (0.5, 4), (0.8, 10), (1.25, 88, "in"), (1.38, 80, "out"), (1.55, 90), (2.0, 89)])
        rr = R(roll)
        belly = TORSO_D * math.cos(rr) + TORSO_W * abs(math.sin(rr)) * 0.92
        floor = belly - SPINE_Z  # pelvis offset that rests the torso on the ground
        st.off = Vector((0.0, keys(t, [(0, 0), (0.25, 0.01), (1.25, -0.25 * TORSO_W)]), max(STAND_R + zr, floor)))
        st.zf = max(STAND_F + zf, floor) if t < 0.9 else floor + (max(STAND_F + zf, floor) - floor) * (1 - smooth((t - 0.9) / 0.35))
        st.roll = rr
        st.limp = smooth((t - 0.75) / 0.5)
        st.neck = (R(keys(t, [(0, 0), (0.3, -12), (0.6, 8), (1.2, -4), (1.4, -14), (2.0, -12)])), 0.0, R(keys(t, [(0, 0), (1.2, 0), (1.6, 10)])))
        st.head = (R(keys(t, [(0, 0), (0.3, -10), (0.7, 6), (1.3, -6), (1.5, 2), (2.0, 0)])), R(keys(t, [(0, 0), (1.2, 0), (1.5, -20)])), 0.0)
        st.tail = (R(keys(t, [(0, 0), (0.6, -14), (1.4, 10), (2.0, 6)])), R(keys(t, [(0, 0), (1.4, 18)])))
        for leg in LEGS:
            st.feet[leg.name] = planted(leg)
        states.append(st)
    return bake("Death", states, loop=False)


# ---------------------------------------------------------------- retargeting a hand-animated quadruped (Barghest)
# The source clips are FBXs exported from Unreal (the QuadrapedCreatures pack's Barghest, a CAT rig facing -Y, metres).
# Body and head rotations carry over as world rotations from rest; feet follow the source's toes, scaled by leg length
# (so a crouch stays a crouch) and pivot like its paws; middle legs (ours can have six) take the front feet's path half
# a step earlier. Loops are retimed to run at this creature's own speeds, so they play near rate 1 in game.
BP = "BARGHEST_ "
B_SRC = {"pelvis": BP + "Pelvis", "chest": BP + "Spine3", "neck": BP + "Neck1", "head": BP + "Head",
         "tail": [BP + "Tail1", BP + "Tail3"]}
B_FEET = {"FL": (BP + "L Finger0", BP + "L Finger01"), "FR": (BP + "R Finger0", BP + "R Finger01"),
          "RL": (BP + "L Foot", BP + "L Toe0"), "RR": (BP + "R Foot", BP + "R Toe0")}
CQ = Quaternion(Z, math.pi / 2)  # source -Y forward, +X left -> our +X forward, +Y left


class Source:
    def __init__(self, path):
        before = set(bpy.data.objects.keys())
        bpy.ops.import_scene.fbx(filepath=path)
        self.obj = next(o for o in bpy.data.objects if o.name not in before and o.type == "ARMATURE")
        for o in [o for o in bpy.data.objects if o.name not in before and o != self.obj]:
            bpy.data.objects.remove(o)
        act = self.obj.animation_data.action
        self.f0, self.f1 = act.frame_range
        self.seconds = (self.f1 - self.f0) / 30.0
        mw = self.obj.matrix_world
        self.rest = {b.name: mw @ b.matrix_local for b in self.obj.data.bones}
        self.rest_tail = {b.name: mw @ b.tail_local for b in self.obj.data.bones}
        self.hip = sum((self.rest[B_FEET[k][0].replace("Foot", "Thigh")].translation.z for k in ("RL", "RR"))) / 2
        self.cache = {}

    def at(self, t):
        """Source pose t seconds in: world matrices (and paw tips), in the source's frame."""
        key = round(t * 600)
        if key not in self.cache:
            fb = self.f0 + t * 30.0
            scene.frame_set(int(fb), subframe=fb - int(fb))
            mw = self.obj.matrix_world
            pose = {pb.name: mw @ pb.matrix for pb in self.obj.pose.bones}
            tips = {pb.name: mw @ pb.tail for pb in self.obj.pose.bones}
            self.cache[key] = (pose, tips)
        return self.cache[key]

    def delta(self, pose, name):
        """World rotation from rest of a source bone, in our axes."""
        d = pose[name].to_quaternion() @ self.rest[name].to_quaternion().inverted()
        return CQ @ d @ CQ.inverted()

    def foot(self, pose, tips, leg):
        ankle_b, toe_b = B_FEET[leg]
        return CQ @ pose[ankle_b].translation, CQ @ tips[toe_b]

    def rest_foot(self, leg):
        ankle_b, toe_b = B_FEET[leg]
        return CQ @ self.rest[ankle_b].translation, CQ @ self.rest_tail[toe_b]

    def speed(self):
        """How fast a planted toe slides back (m/s), from the rear feet."""
        rates = []
        for leg in ("RL", "RR"):
            n = 60
            ps = [self.foot(*self.at(self.seconds * i / n), leg)[1] for i in range(n + 1)]
            zmin = min(p.z for p in ps)
            for i in range(n):
                if ps[i].z < zmin + 0.03 and ps[i + 1].z < zmin + 0.03:
                    rates.append(abs(ps[i + 1].x - ps[i].x) / (self.seconds / n))
        rates.sort()
        return rates[len(rates) // 2] if rates else 0.0

    def phase(self, leg):
        """When in the loop (0..1) this foot is lowest."""
        n = 60
        zs = [self.foot(*self.at(self.seconds * i / n), leg)[1].z for i in range(n)]
        return zs.index(min(zs)) / n


def angle_xz(vec):
    return math.atan2(-vec.z, vec.x)  # toe-down positive


def retarget(name, src, loop, target_cms=None, retime=1.0):
    k = LEG_L / max(1e-6, src.hip)  # source metres -> our units, by leg length
    dur = src.seconds * retime
    if loop and target_cms:
        v_src = src.speed() * k  # our units / s at the source's timing
        v_want = target_cms / HEIGHT_CM
        dur = src.seconds * (v_src / v_want if v_src > 1e-6 else 1.0)
        print(f"RETARGET {name}: source {src.speed():.2f} m/s -> {v_src * HEIGHT_CM:.0f} cm/s here, retimed x{dur / src.seconds:.2f} for {target_cms:.0f} cm/s")
    N = max(8, round(dur * FPS))
    rest_pose = {n: m for n, m in src.rest.items()}
    p0 = CQ @ rest_pose[B_SRC["pelvis"]].translation
    rest_feet = {leg: src.rest_foot(leg) for leg in B_FEET}
    mid_shift = {}
    if loop:
        for side in "LR":
            pf, pr = src.phase("F" + side), src.phase("R" + side)
            gap = (pf - pr) % 1.0
            mid_shift[side] = gap / 2  # the middle foot lands halfway between the hind and the front one
    states = []
    for f in range(N + 1):
        u = f / N  # 0..1 through the clip
        pose, tips = src.at(u * src.seconds)
        st = State()
        st.off = (CQ @ pose[B_SRC["pelvis"]].translation - p0) * k
        st.zf = st.off.z
        st.q_pelvis = src.delta(pose, B_SRC["pelvis"])
        st.q_chest = src.delta(pose, B_SRC["chest"])
        st.q_neck = src.delta(pose, B_SRC["neck"])
        st.q_head = src.delta(pose, B_SRC["head"])
        st.q_tail = [src.delta(pose, b) for b in B_SRC["tail"]]
        for leg in LEGS:
            if leg.pair == "M":
                if loop:
                    up = (u + mid_shift[leg.name[1]]) % 1.0
                    pose_m, tips_m = src.at(up * src.seconds)
                    a, t = src.foot(pose_m, tips_m, "F" + leg.name[1])
                    a0, t0 = rest_feet["F" + leg.name[1]]
                else:
                    af, tf = src.foot(pose, tips, "F" + leg.name[1])
                    ar, tr = src.foot(pose, tips, "R" + leg.name[1])
                    a0f, t0f = rest_feet["F" + leg.name[1]]
                    a0r, t0r = rest_feet["R" + leg.name[1]]
                    a, t, a0, t0 = (af + ar) / 2, (tf + tr) / 2, (a0f + a0r) / 2, (t0f + t0r) / 2
            else:
                a, t = src.foot(pose, tips, leg.name)
                a0, t0 = rest_feet[leg.name]
            alpha = angle_xz(t - a) - angle_xz(t0 - a0)
            toe = leg.toe0 + (t - t0) * k
            ankle = toe - foot_dir(leg, alpha)
            st.feet[leg.name] = (ankle, alpha)
        states.append(st)
    if loop:
        states[-1] = states[0]
    return bake(name, states, loop=loop, tail=False)


def from_barghest(folder):
    def load(n):
        return Source(os.path.join(folder, f"BARGHEST_{n}.fbx"))

    retarget("Idle", load("idleLookAround"), loop=True)
    retarget("Walk", load("walk"), loop=True, target_cms=WALK_CMS)
    retarget("Run", load("run"), loop=True, target_cms=RUN_CMS)
    retarget("Attack", load("biteNormal"), loop=False)
    retarget("Hit", load("getHitNormal"), loop=False)
    retarget("Death", load("deathNormal"), loop=False)


# ---------------------------------------------------------------- build the clips
if motion.startswith("barghest:"):
    from_barghest(motion.split(":", 1)[1])
    for o in [o for o in bpy.data.objects if o.type == "ARMATURE" and o != arm]:
        bpy.data.objects.remove(o)
    for a in [a for a in bpy.data.actions if a.name not in ("Idle", "Walk", "Run", "Attack", "Hit", "Death")]:
        bpy.data.actions.remove(a)  # the source clips must not go into the export
elif motion == "procedural":
    idle()
    locomotion("Walk", WALK_CMS)
    locomotion("Run", RUN_CMS)
    attack()
    hit()
    death()
else:
    raise SystemExit(f"unknown --motion {motion}")

# ---------------------------------------------------------------- preview: contact sheets, side and three-quarter
if preview:
    os.makedirs(preview, exist_ok=True)
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "SINGLE"  # clay: the dark textures hide the legs
    scene.display.shading.single_color = (0.75, 0.72, 0.68)
    scene.display.shading.show_cavity = True
    scene.render.resolution_x, scene.render.resolution_y = 400, 300
    scene.world = bpy.data.worlds.new("w")
    bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0, 0))
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.data.lens = 50
    for act in bpy.data.actions:
        arm.animation_data.action = act
        n = int(act.frame_range[1])
        picks = [1 + round(i * (n - 1) / 11) for i in range(12)]
        tiles = []
        for view, loc in (("side", (0.0, -2.4, 0.35)), ("tq", (1.5, -1.8, 0.75))):
            cam.location = loc
            cam.rotation_euler = (Vector((0, 0, 0.3)) - cam.location).to_track_quat("-Z", "Y").to_euler()
            row = []
            for fr in picks:
                scene.frame_set(fr)
                path = os.path.join(preview, f"_tmp_{act.name}_{view}_{fr}.png")
                scene.render.filepath = path
                bpy.ops.render.render(write_still=True)
                img = bpy.data.images.load(path)
                px = np.array(img.pixels[:]).reshape(img.size[1], img.size[0], 4)
                row.append(px)
                bpy.data.images.remove(img)
                os.remove(path)
            tiles.append(np.concatenate(row[:6], axis=1))
            tiles.append(np.concatenate(row[6:], axis=1))
        sheet = np.concatenate(tiles[::-1], axis=0)
        h, w = sheet.shape[:2]
        img = bpy.data.images.new(f"sheet_{act.name}", w, h, alpha=True)
        img.pixels = sheet.ravel()
        img.filepath_raw = os.path.join(preview, f"{cid}_{act.name}.png")
        img.file_format = "PNG"
        img.save()
        print("PREVIEW", img.filepath_raw)

# ---------------------------------------------------------------- debug: joints over a see-through body, large
debug = opt("--debug")  # e.g. "Walk:1,5,9,13,17,21" -> <preview>/<id>_debug_Walk.png
if debug and preview:
    act_name, fr_list = debug.split(":")
    arm.animation_data.action = bpy.data.actions[act_name]
    red = bpy.data.materials.new("joint")
    red.diffuse_color = (1, 0.1, 0.1, 1)
    for leg in LEGS:
        for part in ("_upper", "_lower", "_foot"):
            b = bones[leg.name + part]
            for tip in ((False, True) if part == "_foot" else (False,)):
                bpy.ops.mesh.primitive_uv_sphere_add(radius=0.012, segments=10, ring_count=6)
                s = bpy.context.object
                s.data.materials.append(red)
                s.parent, s.parent_type, s.parent_bone = arm, "BONE", b.name
                s.location = (0, 0 if tip else -b.length, 0)
                s.matrix_parent_inverse = Matrix()
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_xray, scene.display.shading.xray_alpha = True, 0.5
    scene.render.resolution_x, scene.render.resolution_y = 700, 520
    cam.location = (0.0, -2.2, 0.32)
    cam.rotation_euler = (Vector((0, 0, 0.32)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    row = []
    for fr in [int(x) for x in fr_list.split(",")]:
        scene.frame_set(fr)
        path = os.path.join(preview, f"_tmp_dbg_{fr}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        img = bpy.data.images.load(path)
        row.append(np.array(img.pixels[:]).reshape(img.size[1], img.size[0], 4))
        bpy.data.images.remove(img)
        os.remove(path)
    rows = [np.concatenate(row[i:i + 3], axis=1) for i in range(0, len(row), 3)]
    sheet = np.concatenate(rows[::-1], axis=0)
    img = bpy.data.images.new("dbg", sheet.shape[1], sheet.shape[0], alpha=True)
    img.pixels = sheet.ravel()
    img.filepath_raw = os.path.join(preview, f"{cid}_debug_{act_name}.png")
    img.file_format = "PNG"
    img.save()
    print("DEBUG", img.filepath_raw)

bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={"ARMATURE"}, add_leaf_bones=False,
                         bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
                         bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0, axis_forward="X", axis_up="Z")
print("exported", out, "actions", [a.name for a in bpy.data.actions])
