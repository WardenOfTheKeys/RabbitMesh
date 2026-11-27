#!/usr/bin/env python3
"""Builds the talk frames from the drawn talk pose: art/talk_pose.png -> art/talk/talk_0..3.png.

    python3 tools/make_talk.py [--pose art/talk_pose.png] [--ref art/walk/walk_0.png] [--out art/talk]

He stops walking to say his line: talk_0 is him standing with the radio low at his chest,
talk_1 and talk_2 raise it, talk_3 is the drawing itself, the radio at his mouth. The
firmware plays 0 1 2 3 to raise it, holds 3 while he talks (opening and closing his mouth),
and plays 3 2 1 0 to lower it before he walks on.

All of it is done on the full-size drawing:
  - it is scaled to the walk frames' height and lined up with them (the visor in the same
    column, the boots on the same ground line), so the change from walking doesn't jump;
  - his stance: the drawing has him mid-stride; the far (dark) leg is turned forward about
    its hip so he stands with his feet under him;
  - the arm (sleeve, glove and radio) is cut out by its colours inside the arm's area, so
    his muzzle, mouth and visor stay behind;
  - the UNDERLAY: where the arm covered his chin, neck and the front of his jacket, those
    are painted in from the fur and jacket beside them, row by row, so with the arm lowered
    nothing shows through his shoulder or chest. Outside his body stays transparent;
  - the forearm then turns about the elbow from the chest (talk_0) up to the mouth (talk_3).

The joint positions are for this drawing (POSE below, in its own pixels); change them for
other art. Needs Pillow + numpy.
"""
import argparse
import math
import os
from collections import deque

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))

# art/talk_pose.png (744 x 2112): the redraw in the v5 walk sheet's style, in use. He already
# stands with his feet together, so the far leg isn't turned.
POSE_V5 = {
    "belt_y": 1105,
    "far_hip": (300, 1110),
    "leg_split_x": 330,
    "far_leg_turn": 0.0,
    # the forearm, hand and radio (in this drawing's pixels)
    "arm_area": [(440, 625), (575, 625), (580, 760), (565, 900), (505, 965), (470, 1012), (395, 1012), (395, 930), (430, 860), (432, 700)],
    "elbow": (435, 990),
    "wrist": (470, 880),
    "upright": 0.85,
    "front": [(560, 520), (600, 525), (650, 500), (700, 478), (745, 440), (800, 432), (1000, 432)],
    "lowered": -12.0,
    "lowered_move": (12, 139),                  # how far the whole arm comes down (px, this drawing)
}
POSES = {"v5": POSE_V5}
POSE = POSE_V5
STEPS = [1.0, 0.66, 0.33, 0.0]              # talk_0..3: how far from the drawing's arm angle


def lum(p):
    return 0.3 * p[0] + 0.59 * p[1] + 0.11 * p[2]


def front_x(y):
    pts = POSE["front"]
    if y <= pts[0][0]:
        return pts[0][1]
    for (y0, x0), (y1, x1) in zip(pts, pts[1:]):
        if y0 <= y <= y1:
            return x0 + (x1 - x0) * (y - y0) / max(1, y1 - y0)
    return pts[-1][1]


def legs_split(a, belt_y, split_x):
    """Near and far leg masks below the belt. In this drawing the legs only touch at the
    crotch, so a line between them is enough (colour isn't: the near leg's shading is the
    far leg's purple)."""
    H, W = a.shape[:2]
    op = a[..., 3] > 128
    ys, xs = np.mgrid[0:H, 0:W]
    legs = op & (ys >= belt_y)
    return legs & (xs >= split_x), legs & (xs < split_x)


def legs_split_by_colour(a, belt_y):
    """Near (light purple) and far (dark purple) leg masks below the belt, every leg pixel
    given to the nearer seed by a breadth-first search. Not used for the talk pose (see
    legs_split); kept for drawings whose legs overlap."""
    H, W = a.shape[:2]
    op = a[..., 3] > 128
    r, g, b = a[..., 0].astype(int), a[..., 1].astype(int), a[..., 2].astype(int)
    region = op.copy()
    region[:belt_y] = False
    near_seed = region & (r > 90) & (b > 130)
    far_seed = region & (r > 25) & (r < 85) & (b > 55) & (b < 125) & ~near_seed
    lab = np.zeros((H, W), np.int8)
    q = deque()
    for (ys, xs), v in ((np.nonzero(near_seed), 1), (np.nonzero(far_seed), 2)):
        for y, x in zip(ys, xs):
            lab[y, x] = v
            q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            yy, xx = y + dy, x + dx
            if 0 <= yy < H and 0 <= xx < W and region[yy, xx] and lab[yy, xx] == 0:
                lab[yy, xx] = lab[y, x]
                q.append((yy, xx))
    return lab == 1, lab == 2


def layer(base, mask):
    out = base.copy()
    out[..., 3] = np.where(mask, out[..., 3], 0)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pose", default=os.path.join(HERE, "..", "art", "talk_pose.png"))
    ap.add_argument("--ref", default=os.path.join(HERE, "..", "art", "walk", "walk_0.png"))
    ap.add_argument("--out", default=os.path.join(HERE, "..", "art", "talk"))
    ap.add_argument("--preset", choices=sorted(POSES), default="v5", help="whose joint positions: v5 for art/talk_pose.png")
    a = ap.parse_args()
    global POSE
    POSE = POSES[a.preset]

    src = np.array(Image.open(a.pose).convert("RGBA"))
    H, W = src.shape[:2]
    # The drawing's soft glow (semi-transparent) goes first: the sprite drops it anyway,
    # and turned with a leg it would leave a dotted trace where the leg was.
    src[..., 3] = np.where(src[..., 3] > 128, 255, 0).astype(np.uint8)
    op = src[..., 3] > 128

    # ---- standing: the far leg turned forward about its hip --------------------------------
    near, far = legs_split(src, POSE["belt_y"], POSE["leg_split_x"])
    far_img = Image.fromarray(layer(src, far), "RGBA").rotate(
        POSE["far_leg_turn"], resample=Image.BICUBIC, center=POSE["far_hip"])
    body_wo_far = Image.fromarray(layer(src, ~far), "RGBA")
    stand = Image.new("RGBA", (W, H))
    stand.alpha_composite(far_img)              # behind
    stand.alpha_composite(body_wo_far)          # the body and the near leg in front
    st = np.array(stand)

    # ---- the arm, cut out by colour inside its area -------------------------------------------
    area = Image.new("L", (W, H))
    ImageDraw.Draw(area).polygon(POSE["arm_area"], fill=255)
    area = np.array(area) > 0
    r, g, b = st[..., 0].astype(int), st[..., 1].astype(int), st[..., 2].astype(int)
    opq = st[..., 3] > 128
    fur = (r > 150) & (g > 140) & (b > 150) & (np.abs(r - b) < 60)          # white/pink fur
    mouth = (r > 110) & (g < 100) & (r - b > 40)                            # the open mouth, the nose
    visor = (g > 150) & (b > 150) & (r < 140)
    ys, xs = np.mgrid[0:H, 0:W]
    in_front = xs >= np.vectorize(front_x)(ys) - 2
    arm = area & opq & ~((fur | mouth | visor) & ~in_front)
    # Only the arm itself: the largest connected piece. The rest (the breath marks drawn in
    # front of his mouth) belongs to the talking pose and goes when the radio is lowered.
    comp = np.zeros((H, W), np.int32)
    sizes = [0]
    for y0, x0 in zip(*np.nonzero(arm)):
        if comp[y0, x0]:
            continue
        n = len(sizes)
        comp[y0, x0] = n
        q, c = deque([(y0, x0)]), 0
        while q:
            y, x = q.popleft()
            c += 1
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                yy, xx = y + dy, x + dx
                if 0 <= yy < H and 0 <= xx < W and arm[yy, xx] and not comp[yy, xx]:
                    comp[yy, xx] = n
                    q.append((yy, xx))
        sizes.append(c)
    main_arm = comp == int(np.argmax(sizes))
    extras = arm & ~main_arm
    wy = POSE["wrist"][1]
    hand = main_arm & (ys < wy)                 # glove and radio
    forearm = main_arm & (ys >= wy)             # sleeve, cuff to elbow

    # ---- the underlay: what the arm covered, painted in from beside it --------------------------
    under = layer(st, ~arm)                     # (the breath marks go too: they're in arm)
    for y in range(H):
        row = arm[y]
        if not row.any():
            continue
        fx = front_x(y)
        cols = np.nonzero(row)[0]
        for x in cols:
            if x > fx:
                continue                        # in front of him: air
            # the nearest pixel of his body to the left in this row that isn't arm
            k = x - 1
            while k >= 0 and (arm[y, k] or under[y, k, 3] < 128):
                k -= 1
            if k >= 0:
                under[y, x] = under[y, k]
                under[y, x, 3] = 255
    under_img = Image.fromarray(under, "RGBA")
    fore_pil = Image.fromarray(layer(st, forearm), "RGBA")
    hand_pil = Image.fromarray(layer(st, hand), "RGBA")
    extras_pil = Image.fromarray(layer(st, extras), "RGBA")

    # ---- scale and line up with the walk frames -------------------------------------------------
    ref = np.array(Image.open(a.ref).convert("RGBA"))
    rop = ref[..., 3] > 128
    ry, rx = np.nonzero(rop)
    ref_h = ry.max() - ry.min() + 1
    rvis = rop & (ref[..., 1] > 170) & (ref[..., 2] > 170) & (ref[..., 0] < 120)
    ref_vx = np.nonzero(rvis)[1].mean()
    ty, tx = np.nonzero(op)
    s = ref_h / (ty.max() - ty.min() + 1)
    tvis = op & (src[..., 1] > 170) & (src[..., 2] > 170) & (src[..., 0] < 120)
    t_vx = np.nonzero(tvis)[1].mean()
    out_w, out_h = ref.shape[1], ref.shape[0]

    os.makedirs(a.out, exist_ok=True)
    ex, ey = POSE["elbow"]
    wx, wy = POSE["wrist"]
    for k, f in enumerate(STEPS):
        th = POSE["lowered"] * f
        frame = Image.new("RGBA", (W, H))
        frame.alpha_composite(under_img)
        if f == 0:
            frame.alpha_composite(extras_pil)   # the drawing as drawn, breath marks and all
        # Lowering is the whole arm coming down to his chest (as he holds it walking) with a
        # little turn of the forearm, not the forearm swinging out in front of him.
        mx, my = POSE["lowered_move"][0] * f, POSE["lowered_move"][1] * f
        frame.alpha_composite(fore_pil.rotate(th, resample=Image.BICUBIC, center=(ex, ey), translate=(mx, my)))
        # The hand goes with the forearm, then turns back about the wrist so the radio
        # stays upright: a turn of th*(1 - upright) about the wrist, moved to where the
        # wrist went.
        a_ = math.radians(th)
        ddx, ddy = wx - ex, wy - ey
        nwx = ex + ddx * math.cos(a_) + ddy * math.sin(a_) + mx
        nwy = ey - ddx * math.sin(a_) + ddy * math.cos(a_) + my
        frame.alpha_composite(hand_pil.rotate(th * (1 - POSE["upright"]), resample=Image.BICUBIC,
                                              center=(wx, wy), translate=(nwx - wx, nwy - wy)))
        # turning leaves soft edges: back to solid or clear, as the sprite will be
        fa = np.array(frame)
        fa[..., 3] = np.where(fa[..., 3] > 128, 255, 0).astype(np.uint8)
        frame = Image.fromarray(fa, "RGBA")
        small = frame.resize((max(1, round(W * s)), max(1, round(H * s))), Image.LANCZOS)
        sm = np.array(small)
        sy, sx = np.nonzero(sm[..., 3] > 128)
        dx = round(ref_vx - t_vx * s)
        dy = ry.max() - sy.max()
        canvas = Image.new("RGBA", (out_w, out_h))
        canvas.alpha_composite(small, (dx, dy)) if dx >= 0 and dy >= 0 else canvas.paste(small, (dx, dy), small)
        path = os.path.join(a.out, f"talk_{k}.png")
        canvas.save(path)
        print(f"{path}: arm at {POSE['lowered'] * f:+.0f} deg")


if __name__ == "__main__":
    main()
