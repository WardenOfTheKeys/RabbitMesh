// The Neon Rabbit: RabbitMesh's lock-screen character, from art/rabbit.png (the owner's
// art), turned into a palette sprite by tools/make_sprite.py (rabbit_sprite.h).
//
// He's animated the way Squatch Mesh animates its sasquatch:
//   - he WALKS: `phase` is the stride, as it is for the sasquatch. With walk frames
//     (art/walk, built by tools/make_walk.py or hand-drawn) the frame for this point of the
//     stride is drawn, legs and bob as painted. With only the standing pose, its legs are
//     bent like a paper puppet at the joints the converter found (rabbit_sprite.h, "the
//     rig"): contact, passing with the knee bent, contact, passing with the other foot.
//     Either way his feet stay on the ground line.
//   - his ears sway a beat behind his step, droop when he's tired and perk up when he's
//     happy;
//   - talking, he lifts the radio to his mouth (the radio's screen lights, rings go out of
//     the antenna); waving, he holds it up high and shakes it; arms up, he holds it up;
//   - his visor glows in the eye colour the lock screen gives (the theme's accent, amber
//     with unread messages), pulses, dims when he blinks, shows ^ ^ when happy, sits
//     half-lit when tired; stars circle his head when he's dizzy;
//   - his outline is lit in the theme's second colour, so he stands off a dark scene.
// He takes the same SquatchPose as the sasquatch, so the speech bubble, the hop when he's
// poked and the screen changes (fx_squatch.cpp) all work for him.
//
// x is the middle of his feet, groundY where they stand, h his height in pixels
// (rabbitsprite::H is the drawn size; any other is scaled, nearest-pixel).

#pragma once
#include <math.h>
#include <stdlib.h>
#include "display_config.h"
#include "theme.h"
#include "mascot.h"
#include "rabbit_sprite.h"
#include "holiday_sprite.h"

namespace rabbit {

using namespace rabbitsprite;

// A holiday costume (holiday_sprite.h): while one is set, he's drawn from its frames and palette
// instead of the base ones - the same canvas, frame order and rig, so he walks and talks the same.
inline const holidaysprite::Set*& costumeSet() { static const holidaysprite::Set* s = nullptr; return s; }
// A costume frame, unpacked from its runs into a buffer kept for the next call.
inline const uint8_t* costumeFrame(const holidaysprite::Set& cs, int frame) {
  static uint8_t buf[W * H];
  static const holidaysprite::Set* lastSet = nullptr;
  static int lastFrame = -1;
  if (lastSet == &cs && lastFrame == frame) return buf;
  const uint8_t* r = cs.runs + cs.frameAt[frame];
  const uint8_t* end = cs.runs + cs.frameAt[frame + 1];
  int n = 0;
  while (r < end && n < W * H) {
    const uint8_t code = *r >> 6, v = *r & 63;
    r++;
    int len = code + 1;
    if (code == 3) len = 4 + *r++;
    while (len-- > 0 && n < W * H) buf[n++] = v;
  }
  while (n < W * H) buf[n++] = 0;
  lastSet = &cs; lastFrame = frame;
  return buf;
}

// The colour each palette entry is drawn in this frame.
struct Look { uint16_t pal[NPAL]; };
static_assert(NPAL >= 64, "a costume's palette has up to 64 entries");

inline void look(Look& L, const Theme& t, float phase, uint16_t eye, const SquatchPose& pose, bool talking,
                 const holidaysprite::Set* cs = nullptr) {
  using mascot::mix;
  const float pulse = 0.5f + 0.5f * sinf(phase * 0.55f);
  uint16_t visor = mix(eye, t.white, 0.12f + 0.22f * pulse);
  if (pose.blink) visor = mix(t.bg, eye, 0.28f);
  else if (pose.slump > 0) visor = mix(visor, t.bg, 0.55f * pose.slump);
  const bool ledOn = talking || pose.wave > 0 || fmodf(phase, 8.0f) < 1.1f;   // a blink every few steps
  const uint16_t led = ledOn ? mascot::rgb(0xffb43c) : mix(t.bg, mascot::rgb(0xffb43c), 0.35f);
  const uint16_t rim = mix(t.bg, t.greenDim, 0.44f);           // steady: a pulsing outline repaints it every frame
  const int n = cs ? cs->npal : NPAL;
  const uint8_t* tag = cs ? cs->tag : TAG;
  const uint16_t* base = cs ? cs->pal565 : PAL565;
  for (int i = 0; i < NPAL; i++) {
    if (i >= n) { L.pal[i] = 0; continue; }
    switch (tag[i]) {
      case TAG_VISOR: L.pal[i] = visor; break;
      case TAG_LED:   L.pal[i] = led; break;
      case TAG_RIM:   L.pal[i] = rim; break;
      default:        L.pal[i] = base[i];
    }
  }
}

// The puppet's parts, by where a sprite pixel is.
enum Part : uint8_t { BODY = 0, LEG_BACK, LEG_FRONT, EAR_L, EAR_R, ARM, PARTS };
inline uint8_t partOf(int x, int y) {
  if (x >= ARM_X0 && x <= ARM_X1 && y >= ARM_Y0 && y <= ARM_Y1) return ARM;
  if (y < EAR_Y) return x <= EAR_SPLIT ? EAR_L : EAR_R;
  if (y > HIP_Y) return x <= LEG_SPLIT ? LEG_BACK : LEG_FRONT;
  return BODY;
}

// How one part is moved: the rows [s0, s1] are stretched by `stretch` px at the end away
// from the pivot, the free end is shifted `tip` px sideways (0 at the pivot, a straight
// shear between), and the whole part is moved by (tx, ty).
struct Move {
  int s0, s1;
  bool pivotTop;        // legs hang from the hip; ears and the arm stand on their base
  float stretch, tip;
  int tx, ty;
  float knee;           // legs: the shin below the knee kicks back this many px at the foot (no default: C++11 aggregate)
};
constexpr float KNEE_AT = 0.5f;                      // the knee: halfway down the leg

// Where output row o of a moved part comes from, and how far it's shifted sideways.
inline bool sourceRow(const Move& m, int o, int& sy, int& dx) {
  const int len = m.s1 - m.s0;
  const int st = (int)lroundf(m.stretch);
  const int o0 = m.pivotTop ? m.s0 : m.s0 - st, o1 = m.pivotTop ? m.s1 + st : m.s1;
  if (o < o0 || o > o1) return false;
  const int olen = o1 - o0;
  if (olen <= 0) { sy = m.pivotTop ? m.s0 : m.s1; dx = 0; return true; }
  const float f = (float)(o - o0) / olen;                     // 0 at the top, 1 at the bottom
  sy = m.s0 + (int)lroundf(f * len);
  float sh = m.tip * (m.pivotTop ? f : 1 - f);
  if (m.knee != 0 && f > KNEE_AT) sh -= m.knee * (f - KNEE_AT) / (1 - KNEE_AT);   // he faces right: back is left
  dx = (int)lroundf(sh);
  return true;
}

// Where a point of the arm (the radio's screen) ends up, for the rings out of the antenna.
inline void armPoint(const Move& m, int x, int y, int& ox, int& oy) {
  const int len = m.s1 - m.s0, st = (int)lroundf(m.stretch);
  const int o0 = m.s0 - st, olen = m.s1 - o0;
  const float f = len ? (float)(y - m.s0) / len : 0;           // 0 at the top
  oy = o0 + (int)lroundf(f * olen) + m.ty;
  ox = x + (int)lroundf(m.tip * (1 - f)) + m.tx;
}

// The frame he's composed in before it goes to the canvas in one pushImage: a few
// thousand small rectangles a frame (painted shading makes short runs) cost far more on
// the ESP32 than one clipped image copy. RGB565 with its two bytes swapped, as LovyanGFX
// sprites hold it (fx_internal.h); KEY marks the transparent pixels. Kept between frames,
// in PSRAM when malloc puts it there, grown if a bigger size is asked for.
constexpr uint16_t KEY = 0x2000;                 // swapped 0x0020: a green no palette entry is
inline uint16_t* frameBuf(size_t n) {
  static uint16_t* buf = nullptr;
  static size_t cap = 0;
  if (n > cap) {
    uint16_t* b = (uint16_t*)realloc(buf, n * sizeof(uint16_t));
    if (!b) return nullptr;
    buf = b; cap = n;
  }
  return buf;
}
inline uint16_t swapped(uint16_t c) {
  const uint16_t s = (uint16_t)((c >> 8) | (c << 8));
  return s == KEY ? (uint16_t)(s ^ 0x0100) : s;  // a colour that happens to be the key moves one step of blue
}

// ---- walking between his lines: the lock screen's rabbit ---------------------------------------
// He walks; when the lock screen has him say something he walks on to the next passing pose
// (legs together), stops, raises the radio (the talk frames, 0 -> NTALK-1), talks with his
// mouth following the words, lowers it again, and walks on from where he stopped. While he
// stands the street stands too: the ground only moves with his stride, by exactly what his
// planted foot travels (STRIDE_PX a stride), so his foot stays put on it.
enum : uint8_t { WALK = 0, STOPPING, RAISE, TALK, LOWER };

// How far the street has gone under him at a point in his stride: each drawing moves it by
// exactly what that drawing's planted boot travels (STANCE_DX), not an even share, so the
// boot carrying his weight stays put on the road from one drawing to the next.
inline float groundAt(float walkPhase) {
  float c = walkPhase / (2.0f * (float)M_PI);
  const float cycles = floorf(c);
  const float fc = (c - cycles) * NWALK;
  const int k = ((int)fc) % NWALK;
  float g = cycles * STRIDE_PX;
  for (int i = 0; i < k; i++) g -= STANCE_DX[i];
  return g - STANCE_DX[k] * (fc - floorf(fc));
}
constexpr int TALK_HOLD = 2;                   // lock frames on each raise/lower frame
constexpr int QUIET_FRAMES = 8;                // after the bubble goes, how long before he lowers it

struct Walker {
  float walkPhase = 0;                         // his own stride: moves only while he walks
  float ground = 0;                            // px the street has moved under him
  float last = 0;
  bool started = false;
  uint8_t state = WALK, tf = 0, hold = 0;
  uint16_t quiet = 0;

  int walkFrame() const {
    float c = walkPhase / (2.0f * (float)M_PI);
    c -= floorf(c);
    return ((int)(c * NWALK)) % NWALK;
  }
  void advance(float dp) {
    ground += groundAt(walkPhase + dp) - groundAt(walkPhase);
    walkPhase += dp;
    if (ground > 100000.0f) ground -= 100000.0f;
  }
  // Once per lock-screen frame. phase is the lock screen's own (0.32 a frame).
  void step(float phase, bool wantsTalk) {
    float dp = started ? phase - last : 0;
    started = true;
    last = phase;
    if (dp < 0 || dp > 1.0f) dp = 0;           // the lock screen started its phase again
    if (!WALK_FRAMES || NTALK < 2) { advance(dp); state = WALK; return; }
    switch (state) {
      case WALK:
        advance(dp);
        if (wantsTalk) state = STOPPING;
        break;
      case STOPPING: {                         // walk on to a passing pose, then stop
        advance(dp);
        const int f = walkFrame();
        if (!wantsTalk) state = WALK;
        else if (f == NWALK / 4 || f == 3 * NWALK / 4) { state = RAISE; tf = 0; hold = 0; }
        break;
      }
      case RAISE:
        if (!wantsTalk) { state = LOWER; hold = 0; break; }
        if (++hold >= TALK_HOLD) { hold = 0; if (tf < NTALK - 1) tf++; else { state = TALK; quiet = 0; } }
        break;
      case TALK:
        if (wantsTalk) quiet = 0;
        else if (++quiet > QUIET_FRAMES) { state = LOWER; hold = 0; }
        break;
      case LOWER:
        if (wantsTalk) { state = RAISE; hold = 0; break; }
        if (++hold >= TALK_HOLD) { hold = 0; if (tf > 0) tf--; else state = WALK; }
        break;
    }
  }
  bool walking() const { return state == WALK || state == STOPPING; }
};
inline Walker& walker() { static Walker w; return w; }

// What to draw this frame.
struct Shot {
  int frame;            // into PIX
  float sub;            // 0..1: how far through this walk drawing (the planted foot slides with it)
  int bob;              // px up
  float lean;           // px forward at the top of his head
  float walkPhase;      // for the ears' sway
  bool mouthOpen;
  bool walking;
  bool overlays;        // the antenna's rings, the ^ ^, the stars (off for checking the art alone)
};


// Walking, from a stride phase: the drawing, the bob (down at contact, up passing: 0 1 2 1 over
// each half), and the lean (upright as a foot lands, forward as his weight goes over it - the
// sasquatch's lean surges the same way).
inline Shot walkShot(float walkPhase, float tired) {
  Shot s;
  float c = walkPhase / (2.0f * (float)M_PI);
  c -= floorf(c);
  const float fc = c * NWALK;
  s.frame = WALK_FRAMES ? ((int)fc) % NWALK : 0;
  s.sub = fc - floorf(fc);
  const float bobPeak = 2.0f * (1 - 0.5f * tired);
  s.bob = (int)lroundf(fabsf(sinf(2.0f * (float)M_PI * s.frame / NWALK)) * bobPeak);
  s.lean = (1.0f + 2.0f * fabsf(sinf(walkPhase))) * (1 - 0.5f * tired);
  s.walkPhase = walkPhase;
  s.mouthOpen = false;
  s.walking = true;
  s.overlays = true;
  return s;
}

}  // namespace rabbit

namespace rabbit {
// Where the rings out of the antenna start, from the last drawing that showed the radio's
// screen (screen coordinates of whatever he was drawn on). The boot screen animates them alone.
struct Point { int x, y; };
inline Point& ledPoint() { static Point p = {-1, -1}; return p; }
// The visor's centre in the last drawing (screen coordinates): things drawn on him (a
// scarf) are drawn from it, so they bob and lean with him.
inline Point& visorPoint() { static Point p = {-1, -1}; return p; }
}  // namespace rabbit

template <class G>
inline void drawRabbitShot(G& d, const Theme& t, int x, int groundY, int h, float phase,
                           uint16_t eyeColor, const SquatchPose& pose, const rabbit::Shot& shot) {
  using namespace rabbitsprite;
  using namespace rabbit;
  using mascot::mix;
  if (h <= 0) return;
  const bool talking = !shot.walking;
  const holidaysprite::Set* costume = rabbit::costumeSet();     // a holiday costume, or the base frames
  const uint8_t* tagOf = costume ? costume->tag : TAG;
  const int npal = costume ? costume->npal : NPAL;
  Look L;
  look(L, t, phase, eyeColor, pose, talking, costume);
  for (int i = 0; i < npal; i++)                                // the mouth: open on the words
    if (tagOf[i] == TAG_MOUTH && !shot.mouthOpen) L.pal[i] = costume ? costume->pal565[costume->furIdx] : PAL565[FUR_IDX];
  const uint8_t* pix = costume ? costumeFrame(*costume, shot.frame) : PIX[shot.frame];

  // ---- the pose this frame --------------------------------------------------------------
  const int frame = shot.frame;
  const bool inWalk = WALK_FRAMES && frame < NWALK;
  const float tired = pose.slump;
  const float wp = shot.walkPhase;
  const float sn = sinf(wp), cs = cosf(wp);
  const int bob = shot.bob;
  const int leanTop = (int)lroundf(shot.lean);
  Move mv[PARTS];
  // Body: rises with the bob, sinks a pixel when tired, leans forward from the hip.
  mv[BODY] = {0, HIP_Y, false, 0, shot.lean, 0, -bob + (tired > 0.5f ? 1 : 0), 0};
  // Legs. With walk frames they're as drawn (the planted foot slides below); with only the
  // standing pose they're bent like a puppet, as before.
  const float stride = WALK_FRAMES ? 0 : 10.0f * (1 - 0.6f * tired);
  const float liftBack = WALK_FRAMES ? 0 : (cs > 0 ? cs * 4.0f : 0), liftFront = WALK_FRAMES ? 0 : (cs < 0 ? -cs * 4.0f : 0);
  const float open = fabsf(sn) * stride;
  mv[LEG_BACK]  = {HIP_Y + 1, H - 1, true, bob - liftBack + (tired > 0.5f ? -1 : 0), -open * 0.8f, 0, mv[BODY].ty, liftBack * 1.8f};
  mv[LEG_FRONT] = {HIP_Y + 1, H - 1, true, bob - liftFront + (tired > 0.5f ? -1 : 0), open, 0, mv[BODY].ty, liftFront * 1.8f};
  // Ears: sway a beat behind the step (still when he stands), droop when tired, perk when happy;
  // they lean with his head.
  const float sway = shot.walking ? 2.5f : 0.6f;
  const float earL = sinf(wp - 0.9f) * sway - 4.0f * tired, earR = sinf(wp - 1.5f) * sway - 3.0f * tired;
  const float perk = pose.happy ? 1.0f : 0;
  mv[EAR_L] = {0, EAR_Y - 1, false, perk - 2.0f * tired, earL, leanTop, mv[BODY].ty, 0};
  mv[EAR_R] = {0, EAR_Y - 1, false, perk - 1.0f * tired, earR, leanTop, mv[BODY].ty, 0};
  // The radio arm moves only as drawn (the talk frames raise it, over a painted-in jacket), so
  // nothing can open a hole in his chest. It leans with his body.
  const int armLean = (int)lroundf(shot.lean * (1.0f - 0.5f * (ARM_Y0 + ARM_Y1) / (float)HIP_Y));
  mv[ARM] = {ARM_Y0, ARM_Y1, false, 0, 0, armLean, mv[BODY].ty, 0};

  // ---- drawing: each part's rows into the frame, runs of one colour at a time ----------
  const int dh = h, dw = (W * h + H / 2) / H;
  const int left = x - (FOOT_X * dw) / W, top = groundY - dh + 1;
  const int padX = (20 * dw) / W + 2, padY = (14 * dh) / H + 2;
  const int bw = dw + 2 * padX, bh = dh + 2 * padY;
  uint16_t* buf = frameBuf((size_t)bw * bh);
  if (!buf) return;
  for (int i = 0; i < bw * bh; i++) buf[i] = KEY;
  uint16_t sw[NPAL];
  for (int i = 0; i < NPAL; i++) sw[i] = swapped(L.pal[i]);
  int ledX = -1, ledY = -1;                                     // where the radio's screen ended up
  auto emit = [&](int sx0, int len, int oy, uint8_t idx) {
    int x0 = padX + (sx0 * dw) / W, x1 = padX + ((sx0 + len) * dw) / W;
    int y0 = padY + (oy * dh) / H, y1 = padY + ((oy + 1) * dh) / H;
    if (x0 < 0) x0 = 0; if (x1 > bw) x1 = bw; if (y0 < 0) y0 = 0; if (y1 > bh) y1 = bh;
    for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) buf[yy * bw + xx] = sw[idx];
    if (tagOf[idx] == TAG_LED && ledX < 0) { ledX = sx0; ledY = oy; }
  };
  // The planted foot: in a walk drawing, the boot carrying his weight (and the shin above it,
  // less and less up to the knee) slides back with the street, a little every frame, by what it
  // moves before the next drawing.
  // At a contact the trailing boot is down too: it moves with the ground (a drawing's share of
  // the stride) until the next drawing lifts it.
  const bool plant = inWalk && STANCE_X1[frame] >= STANCE_X0[frame];
  const bool trail = inWalk && TRAIL_X1[frame] >= TRAIL_X0[frame];
  const float trailDx = inWalk ? STANCE_DX[frame] : 0;   // the street's pace during this drawing
  auto plantShift = [&](int sx, int sy) -> int {
    if (sy < KNEE_Y) return 0;
    const float k = shot.sub * (sy - KNEE_Y) / (float)(H - 1 - KNEE_Y);
    if (plant && sx >= STANCE_X0[frame] && sx <= STANCE_X1[frame]) return (int)lroundf(STANCE_DX[frame] * k);
    if (trail && sx >= TRAIL_X0[frame] && sx <= TRAIL_X1[frame]) return (int)lroundf(trailDx * k);
    return 0;
  };
  const bool backLegForward = liftBack > liftFront;
  const uint8_t ORDER[PARTS] = {backLegForward ? (uint8_t)LEG_FRONT : (uint8_t)LEG_BACK, EAR_R, BODY, EAR_L,
                                backLegForward ? (uint8_t)LEG_BACK : (uint8_t)LEG_FRONT, ARM};
  for (uint8_t pi = 0; pi < PARTS; pi++) {
    const uint8_t part = ORDER[pi];
    const bool isLeg = part == LEG_BACK || part == LEG_FRONT;
    const Move& m = mv[part];
    const int o0 = m.pivotTop ? m.s0 : m.s0 - (int)lroundf(m.stretch);
    const int o1 = m.pivotTop ? m.s1 + (int)lroundf(m.stretch) : m.s1;
    for (int o = o0; o <= o1; o++) {
      int sy, dx;
      if (!sourceRow(m, o, sy, dx) || sy < 0 || sy >= H) continue;
      const int oy = o + m.ty;
      if (oy < -8 || oy >= H + 8) continue;               // a part may move past the sprite's own edges
      const uint8_t* row = &pix[sy * W];
      int run = 0, runX = 0, curShift = 0;
      uint8_t cur = 0;
      for (int sx = 0; sx <= W; sx++) {
        uint8_t idx = 0;
        if (sx < W && row[sx]) {
          uint8_t p = partOf(sx, sy);
          if (costume && (p == EAR_L || p == EAR_R || p == ARM)) p = BODY;   // a costume's hat and arm move with him
          if (p == part) idx = row[sx];
        }
        const int sh = (isLeg && idx) ? plantShift(sx, sy) : 0;
        if (idx && idx == cur && sh == curShift) { run++; continue; }
        if (cur) emit(runX + dx + m.tx + curShift, run, oy, cur);
        cur = idx; run = 1; runX = sx; curShift = sh;
      }
    }
  }

  // One copy to the canvas, clipped by it (a screen change's strip included), KEY left out.
  d.pushImage(left - padX, top - padY, bw, bh, (const lgfx::swap565_t*)buf, lgfx::swap565_t(KEY));

  // ---- what's drawn over him ---------------------------------------------------------------
  const float fx = (float)dw / W, fy = (float)dh / H;
  if (ledX >= 0) { rabbit::ledPoint().x = left + (int)(ledX * fx); rabbit::ledPoint().y = top + (int)((ledY - 6) * fy); }
  const int visX = costume ? costume->visorX : VISOR_X, visY = costume ? costume->visorY : VISOR_Y;
  const int vx = left + (int)((visX + leanTop * 0.8f) * fx), vy = top + (int)((visY + mv[BODY].ty) * fy);
  rabbit::visorPoint().x = vx; rabbit::visorPoint().y = vy;
  if (!shot.overlays) return;
  if (pose.happy && !pose.blink) {                              // ^ ^ on the visor
    for (int k = -1; k <= 1; k += 2) {
      const int ex = vx + k * (int)(4 * fx);
      d.drawLine(ex - 2, vy + 1, ex, vy - 1, t.white);
      d.drawLine(ex, vy - 1, ex + 2, vy + 1, t.white);
    }
  }
  // Rings out of the antenna while he talks, from wherever the radio's screen was drawn.
  if (talking && shot.frame == NWALK + NTALK - 1 && ledX >= 0) {
    const int ax = left + (int)(ledX * fx), ay = top + (int)((ledY - 6) * fy);
    for (int k = 0; k < 2; k++) {
      const float f = fmodf(phase * 0.5f + k * 0.5f, 1.0f);
      const int r = 3 + (int)(f * 9);
      const uint16_t c = mix(t.bg, t.green, 0.8f * (1 - f));
      for (int a = -50; a <= 50; a += 10) {
        const float q = (a - 45) * 0.01745f;
        d.drawPixel(ax + (int)(cosf(q) * r), ay + (int)(sinf(q) * r), c);
      }
    }
  }
  if (pose.dizzy >= 0) {                                        // stars round his head
    for (int k = 0; k < 3; k++) {
      const float a = pose.dizzy + k * 2.094f;
      mascot::star(d, vx + (int)(cosf(a) * 12 * fx), vy - (int)(14 * fy) + (int)(sinf(a) * 4), 1, k ? t.amber : t.green);
    }
  }
}

// Walking, wherever a stride phase puts him (the screen changes; anything without the lock
// screen's walk-and-talk).
template <class G>
inline void drawRabbitT(G& d, const Theme& t, int x, int groundY, int h,
                        float phase, uint16_t eyeColor, const SquatchPose& pose = SquatchPose()) {
  drawRabbitShot(d, t, x, groundY, h, phase, eyeColor, pose, rabbit::walkShot(phase, pose.slump));
}

inline void drawRabbit(lgfx::LovyanGFX& d, const Theme& t, int x, int groundY, int h,
                       float phase, uint16_t eyeColor, const SquatchPose& pose = SquatchPose()) {
  drawRabbitT(d, t, x, groundY, h, phase, eyeColor, pose);
}

// The lock screen's rabbit: steps his walk-and-talk once a frame and draws him (scene_rabbit.h).
// The talk frames come in when the lock screen has him say something (pose.talking).
inline void drawRabbitLock(lgfx::LovyanGFX& d, const Theme& t, int x, int groundY, int h,
                           float phase, uint16_t eyeColor, const SquatchPose& pose) {
  using namespace rabbit;
  Walker& w = walker();
  w.step(phase, pose.talking);
  Shot s = walkShot(w.walkPhase, pose.slump);
  if (!w.walking()) {
    s.frame = NWALK + w.tf;
    s.sub = 0; s.bob = 0; s.lean = 0;
    s.walking = false;
    s.mouthOpen = w.state == TALK && pose.mouth != 0;
  }
  drawRabbitShot(d, t, x, groundY, h, phase, eyeColor, pose, s);
}

// The theme's character: the rabbit in Neon Rabbit, the sasquatch everywhere else. h is
// the sasquatch's shoulder height; the rabbit, ears and all, is drawn about as tall again
// as that, which is how the two stand side by side in the screen changes.
inline void drawThemeMascot(lgfx::LovyanGFX& d, const Theme& t, int x, int groundY, int h,
                            float phase, uint16_t eyeColor, const SquatchPose& pose = SquatchPose()) {
  if (rabbitScene(t.scene)) drawRabbit(d, t, x, groundY, h * rabbitsprite::H / 88, phase, eyeColor, pose);
  else drawSasquatch(d, t, x, groundY, h, phase, eyeColor, pose);
}
