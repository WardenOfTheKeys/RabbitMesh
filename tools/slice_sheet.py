#!/usr/bin/env python3
"""Cuts a walk-cycle sprite sheet into art/walk/walk_0..N.png, the frames
tools/make_sprite.py turns into the firmware's walk.

    python3 tools/slice_sheet.py SHEET.png --cols 4 --rows 2 [--out art/walk] [--ref art/rabbit.png]

The sheet is a grid of equal cells read row by row, left to right, one frame of the walk
in each, drawn walking in place (the body at the same spot in every cell, the feet on the
same ground line) with a transparent background. Every frame is cut to the same cell size
so they stay lined up. --ref also writes frame 0 as the reference pose the converter takes
the joints and the visor from. The cuts are checked: a frame whose visor or feet sit
somewhere else than the others' is reported, because it would make him jump on screen.
Needs Pillow + numpy.
"""
import argparse
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sheet")
    ap.add_argument("--cols", type=int, required=True)
    ap.add_argument("--rows", type=int, required=True)
    ap.add_argument("--out", default=os.path.join(HERE, "..", "art", "walk"))
    ap.add_argument("--ref", default=None)
    a = ap.parse_args()

    sheet = Image.open(a.sheet).convert("RGBA")
    cw, ch = sheet.width // a.cols, sheet.height // a.rows
    os.makedirs(a.out, exist_ok=True)
    for old in os.listdir(a.out):
        if old.startswith("walk_") and old.endswith(".png"):
            os.remove(os.path.join(a.out, old))
    marks = []
    for k in range(a.cols * a.rows):
        r, c = divmod(k, a.cols)
        x0 = round(c * sheet.width / a.cols)
        y0 = round(r * sheet.height / a.rows)
        cell = sheet.crop((x0, y0, x0 + cw, y0 + ch))
        px = np.array(cell).astype(int)
        op = px[..., 3] > 128
        if not op.any():
            raise SystemExit(f"cell {k} (row {r}, column {c}) is empty")
        vis = op & (px[..., 1] > 170) & (px[..., 2] > 170) & (px[..., 0] < 120)
        vx = float(np.nonzero(vis)[1].mean()) if vis.any() else float("nan")
        feet = int(np.nonzero(op)[0].max())
        marks.append((vx, feet))
        cell.save(os.path.join(a.out, f"walk_{k}.png"))
        if k == 0 and a.ref:
            cell.save(a.ref)
        print(f"walk_{k}.png: visor x {vx:.0f}, feet at {feet}")
    vxs = [m[0] for m in marks if m[0] == m[0]]
    feet = [m[1] for m in marks]
    if vxs and max(vxs) - min(vxs) > 4:
        print(f"warning: the visor moves {max(vxs) - min(vxs):.0f} px between frames - is the sheet walking in place?")
    if max(feet) - min(feet) > 4:
        print(f"warning: the feet's lowest point moves {max(feet) - min(feet)} px between frames")


if __name__ == "__main__":
    main()
