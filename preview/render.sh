#!/usr/bin/env bash
# Renders the Neon Rabbit lock screen (scene, speech bubble, clock), the rabbit's poses and
# the deck screens (with
# sample data) on a computer, through the real headers and a small stand-in for
# LovyanGFX (mock/). For working on the look without flashing. Needs a built source
# tree (this repository, by default) and g++; PNGs need Python + Pillow (text uses DejaVu Sans Mono).
#
#   ./render.sh [source tree] [output dir]
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/..}
OUT=${2:-$HERE/out}
STAGE=$(mktemp -d)
mkdir -p "$OUT"
cp "$BUILD"/src/{theme.h,themes.h,mascot.h,scenes.h,scene_halloween.h,scene_squatchmas.h,scene_rabbit.h,scene_cyber.h,scene_holiday.h,costume_sprite.h,holiday_sprite.h,holiday_sprite.cpp,rabbit.h,rabbit_sprite.h,rabbit_sprite.cpp,squatch_talk.h,rabbit_talk.h,deck_core.h,deck_draw.h,spectrum_core.h,mecp_core.h,bricks_core.h,bricks_draw.h,bricks_art.h,bricks_art.cpp} "$STAGE"/
cp "$HERE"/mock/* "$STAGE"/          # after the real headers: the mocks replace display_config.h and audio_jingle.h
for r in render render_deck render_lock render_game; do
  g++ -std=c++17 -Wno-misleading-indentation -I"$STAGE" "$HERE/$r.cpp" "$STAGE/rabbit_sprite.cpp" "$STAGE/holiday_sprite.cpp" "$STAGE/bricks_art.cpp" -o "$STAGE/$r"
  (cd "$OUT" && "$STAGE/$r" > /dev/null)
done
# The walk: 40 frames of the lock screen at its own pace, then (with Pillow) a GIF of them.
mkdir -p "$OUT/walk"
(cd "$OUT/walk" && "$STAGE/render_lock" walk > /dev/null)
if python3 -c "import PIL" 2>/dev/null; then
  python3 "$HERE/compose.py" "$OUT"
  python3 "$HERE/compose.py" "$OUT/walk"
  python3 - "$OUT/walk" <<'PY'
import sys
from PIL import Image
d = sys.argv[1]
f = [Image.open(f"{d}/walk_{i:02d}.png").convert("RGB").resize((480, 360), Image.LANCZOS).quantize(colors=64) for i in range(40)]
f[0].save(f"{d}/lock-walk.gif", save_all=True, append_images=f[1:], duration=55, loop=0, optimize=True)
PY
fi
echo "rendered to $OUT"
