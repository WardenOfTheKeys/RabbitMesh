#!/usr/bin/env bash
# Tests the rabbit's animation against Squatch Mesh's sasquatch, on a computer:
#   1. the same lock-screen frames at the same pace for both (home.cpp: phase +0.32,
#      scroll +2 a frame): stride length, bob, how much changes a frame, draw calls;
#   2. a stress test of the puppet under AddressSanitizer/UBSan: every pose, sizes 10-200,
#      positions half off the screen - and nothing drawn outside the box it may use;
#   3. the planted foot (plant.cpp): how far the foot carrying the weight slides against the
#      street a frame, and the lean, for both;
#   4. the walk -> talk -> walk transition on the lock screen (walk_talk.cpp): every talk frame
#      at 44 x 124 and at full width over a bright checkerboard (holes in the jacket show), and
#      110 lock-screen frames through stop, raise, talk, lower and walk on;
#   5. (with Pillow) a side-by-side GIF of the lock screens, a stride strip of both, and a
#      close-up of the two characters alone with guides at the top of the bob and the ground.
#   ./check.sh [source tree] [output dir]
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${1:-$HERE/../build}
OUT=${2:-$HERE/out/compare}
STAGE=$(mktemp -d)
mkdir -p "$OUT"
cp "$BUILD"/src/{theme.h,themes.h,mascot.h,scenes.h,scene_halloween.h,scene_squatchmas.h,scene_rabbit.h,scene_cyber.h,scene_holiday.h,costume_sprite.h,holiday_sprite.h,holiday_sprite.cpp,rabbit.h,rabbit_sprite.h,rabbit_sprite.cpp,squatch_talk.h,rabbit_talk.h} "$STAGE"/
cp "$HERE"/mock/* "$STAGE"/
F="-std=gnu++11 -Wno-misleading-indentation -Wno-missing-field-initializers -Wno-unused-function -I$STAGE"
g++ $F -O1 "$HERE/compare.cpp" "$STAGE/rabbit_sprite.cpp" "$STAGE/holiday_sprite.cpp" -o "$STAGE/compare"
g++ $F -O1 "$HERE/strips.cpp" "$STAGE/rabbit_sprite.cpp" "$STAGE/holiday_sprite.cpp" -o "$STAGE/strips"
g++ $F -O1 "$HERE/closeup.cpp" "$STAGE/rabbit_sprite.cpp" "$STAGE/holiday_sprite.cpp" -o "$STAGE/closeup"
g++ $F -O1 "$HERE/plant.cpp" "$STAGE/rabbit_sprite.cpp" "$STAGE/holiday_sprite.cpp" -o "$STAGE/plant"
g++ $F -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined "$HERE/walk_talk.cpp" "$STAGE/rabbit_sprite.cpp" "$STAGE/holiday_sprite.cpp" -o "$STAGE/walk_talk"
g++ $F -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined "$HERE/stress.cpp" "$STAGE/rabbit_sprite.cpp" "$STAGE/holiday_sprite.cpp" -o "$STAGE/stress"
cd "$OUT"
"$STAGE/compare"
"$STAGE/stress"
"$STAGE/strips"
"$STAGE/closeup"
"$STAGE/plant"
mkdir -p walk_talk && (cd walk_talk && "$STAGE/walk_talk" && cat qa_report.txt)
if python3 -c "import PIL" 2>/dev/null; then
  python3 "$HERE/compose.py" .
  python3 - <<'PY'
from PIL import Image, ImageDraw, ImageFont
try: f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf", 22)
except OSError: f = ImageFont.load_default()
fr = []
for i in range(40):
    a = Image.open(f"sq_{i:02d}.png").convert("RGB").resize((480, 360), Image.LANCZOS)
    b = Image.open(f"rb_{i:02d}.png").convert("RGB").resize((480, 360), Image.LANCZOS)
    s = Image.new("RGB", (970, 400), (12, 12, 16)); s.paste(a, (0, 40)); s.paste(b, (490, 40))
    d = ImageDraw.Draw(s); d.text((10, 8), "Squatch Mesh (sasquatch)", fill=(61, 255, 168), font=f); d.text((500, 8), "RabbitMesh (rabbit)", fill=(0, 240, 255), font=f)
    fr.append(s.quantize(colors=96))
fr[0].save("side-by-side.gif", save_all=True, append_images=fr[1:], duration=55, loop=0, optimize=True)
a = Image.open("strip_squatch.ppm").convert("RGB").crop((0, 90, 320, 205)).resize((960, 345), Image.NEAREST)
b = Image.open("strip_rabbit.ppm").convert("RGB").crop((0, 70, 320, 205)).resize((960, 405), Image.NEAREST)
s = Image.new("RGB", (960, 800), (12, 12, 16)); s.paste(a, (0, 30)); s.paste(b, (0, 395))
d = ImageDraw.Draw(s); d.text((10, 2), "Squatch: one stride, 6 steps", fill=(61, 255, 168), font=f); d.text((10, 368), "RabbitMesh: one stride, 6 steps", fill=(0, 240, 255), font=f)
s.save("strides.png")
# The close-up: both characters alone, guides at the highest the head gets walking and at
# the ground (closeup.cpp measured them into guides.txt).
G = {l.split()[0]: (int(l.split()[1]), int(l.split()[2])) for l in open("guides.txt")}
def panel(name, i, colour, Z=3):
    top, bottom = G[name]
    box = (10, top - 16, 150, bottom + 6)
    im = Image.open(f"cu_{name}_{i:02d}.ppm").convert("RGB").crop(box).resize(((box[2] - box[0]) * Z, (box[3] - box[1]) * Z), Image.NEAREST)
    d = ImageDraw.Draw(im)
    hy = (top - box[1]) * Z
    for x in range(0, im.width, 12): d.line([(x, hy), (x + 6, hy)], fill=colour, width=2)
    d.line([(0, (bottom - box[1]) * Z + Z), (im.width, (bottom - box[1]) * Z + Z)], fill=colour, width=2)
    return im
ha, hb = panel("sq", 0, (0, 0, 0)).height, panel("rb", 0, (0, 0, 0)).height
fr = []
for i in range(40):
    a = panel("sq", i, (61, 255, 168)); b = panel("rb", i, (0, 240, 255))
    s = Image.new("RGB", (a.width + b.width + 20, max(ha, hb) + 70), (12, 12, 16))
    s.paste(a, (0, 40 + max(ha, hb) - ha)); s.paste(b, (a.width + 20, 40 + max(ha, hb) - hb))
    d = ImageDraw.Draw(s)
    d.text((8, 8), "Squatch Mesh", fill=(61, 255, 168), font=f)
    d.text((a.width + 28, 8), "RabbitMesh", fill=(0, 240, 255), font=f)
    label = "walking" if i < 14 else ("talking" if i < 32 else "waving: a message")
    d.text((8, s.height - 22), f"dashed: highest the head gets walking   solid: the ground   [{label}]", fill=(150, 150, 160))
    fr.append(s.quantize(colors=96))
fr[0].save("closeup-side-by-side.gif", save_all=True, append_images=fr[1:], duration=70, loop=0, optimize=True)
PY
fi
echo "compared in $OUT"
