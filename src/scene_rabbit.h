// The Neon Rabbit theme's lock scene: a city at night in the rain. A synthwave sun
// sinks behind two layers of towers sliding past at their own speeds, windows lit cyan,
// magenta and sodium yellow, rooftop beacons blinking, a neon sign or two that flickers,
// rain streaking down, and the rabbit walking the wet street with his radio, the way the
// sasquatch walks his trail: he steps in place and the city goes by. With unread messages
// a billboard on the near layer lights up amber.
//
// Included by scenes.h inside namespace scenes, after its helpers (W, GROUND, NARROW,
// MASCOT_X, rgb, mix, hash, mascotLift, mascotPose). rabbit.h is included at the top of
// scenes.h, outside the namespace, so drawRabbit is global like drawSasquatch. Composed for the T-Deck's 320 px
// band and the pager's 480 px one from the same numbers.

// Where he stands (the middle of his feet): left of centre, so what he says has the
// right-hand side of the screen.
constexpr int RABBIT_X = NARROW ? 64 : 150;
// The speech bubble's tail: in front of his face, at the level of his mouth, and far
// enough right that the bubble clears an ear sticking out forward (squatch_talk.h asks).
inline void rabbitAnchor(int lift, int& ax, int& ay) {
  using namespace rabbitsprite;
  ax = RABBIT_X - FOOT_X + VISOR_X + 20;
  ay = GROUND - H + 1 + VISOR_Y + 4 - lift;
}

inline int neonWrap(float v, int span) {
  int x = (int)v % span;
  return x < 0 ? x + span : x;
}

// One layer of towers. pitch is the spacing, base the ground they stand on, tall the
// tallest; near layers are darker, with more windows lit and the occasional sign.
inline void neonTowers(lgfx::LovyanGFX& d, const Theme& t, float scroll, float phase, int pitch,
                       int base, int tall, uint16_t wall, bool near, bool unread, int seed) {
  const int n = W / pitch + 3, span = n * pitch;
  const uint16_t winC[3] = {t.green, t.greenDim, rgb(0xffb648)};
  for (int i = 0; i < n; i++) {
    const int x = neonWrap(i * pitch - scroll, span) - pitch;
    const uint32_t h = hash(i * 31 + seed);
    const int w = pitch - 4 - (int)(h % 6);
    const int hh = tall / 2 + (int)((h >> 4) % (tall / 2));
    const int top = base - hh;
    d.fillRect(x, top, w, hh, wall);
    // a stepped crown on some
    if ((h >> 9) & 1) d.fillRect(x + w / 4, top - 6, w / 2, 6, wall);
    // windows: a grid, some lit
    const int cols = max(1, (w - 4) / 5), rows = max(1, (hh - 6) / 6);
    for (int r = 0; r < rows; r++) {
      for (int k = 0; k < cols; k++) {
        const uint32_t wh = hash((i * 97 + r * 13 + k) ^ seed);
        const bool lit = (wh % 100) < (near ? 26u : 14u);
        if (!lit) continue;
        // an occasional window flickers off and on
        if ((wh >> 8) % 23 == 0 && sinf(phase * 2.3f + wh) > 0.6f) continue;
        const uint16_t c = winC[(wh >> 12) % 3];
        d.fillRect(x + 3 + k * 5, top + 4 + r * 6, 2, 2, near ? c : mix(wall, c, 0.45f));
      }
    }
    // rooftop beacon
    if ((h >> 11) % 3 == 0) {
      const int bx = x + w / 2, by = top - ((h >> 9) & 1 ? 6 : 0);
      d.drawFastVLine(bx, by - 8, 8, mix(wall, t.dim, 0.6f));
      if (fmodf(phase * 0.6f + (h & 7), 3.0f) < 1.0f) d.fillCircle(bx, by - 9, 1, t.red);
    }
    if (!near) continue;
    // a vertical neon sign on the side of some near towers
    if ((h >> 14) % 4 == 0 && hh > 40) {
      const int sx = x + w - 2, sy = top + 8;
      const bool on = !((h >> 18) % 3 == 0 && sinf(phase * 7.0f + i) > 0.3f);     // some buzz
      const uint16_t sc = ((h >> 16) & 1) ? t.greenDim : t.green;
      d.fillRect(sx, sy, 5, 26, on ? mix(t.bg, sc, 0.35f) : wall);
      d.drawRect(sx, sy, 5, 26, on ? sc : mix(wall, sc, 0.3f));
      if (on) for (int k = 0; k < 4; k++) d.drawFastHLine(sx + 1, sy + 4 + k * 6, 3, mix(sc, t.white, 0.5f));
    }
    // the billboard: lit amber with something unread, dark otherwise
    if (i % 4 == 1 && hh > 30) {
      const int bx = x + 3, by = top + 6, bw = min(w - 6, 22);
      const uint16_t bc = unread ? t.amber : mix(wall, t.dim, 0.3f);
      d.drawRect(bx, by, bw, 10, bc);
      if (unread && fmodf(phase, 1.6f) < 1.1f) {
        d.fillRect(bx + 2, by + 2, bw - 4, 6, mix(t.bg, t.amber, 0.4f));
        d.fillRect(bx + bw / 2 - 1, by + 2, 2, 4, t.amber);                         // a "!"
        d.drawPixel(bx + bw / 2, by + 7, t.amber);
      }
    }
  }
}

inline void neonRabbit(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread) {
  const int g = GROUND;
  // Sky: haze banding down to the horizon, the theme's second colour bleeding up.
  for (int k = 0; k < 6; k++) {
    const int y0 = g - 70 + k * 10;
    d.fillRect(0, y0, W, 10, mix(t.bg, t.greenDim, 0.04f + k * 0.035f));
  }
  stars(d, t.dim, phase, 30, g - 80);
  // The synthwave sun, cut by stripes, low behind the city.
  {
    const int sx = NARROW ? W - 92 : W - 130, sy = g - 58, r = NARROW ? 32 : 40;
    const uint16_t top = rgb(0xffd23f), bottom = t.greenDim;
    for (int y = -r; y <= 0; y++) {
      const int half = (int)sqrtf((float)(r * r - y * y));
      const int up = -y;                                   // px above the sun's centre line
      if (up < r / 2 && up % 6 < 2 + (r / 2 - up) / 8) continue;   // stripes, wider toward the bottom
      d.drawFastHLine(sx - half, sy + y, half * 2, mix(bottom, top, (float)up / r));
    }
  }
  // Far towers, then near ones, sliding past at their own speeds.
  // He walks, the city goes by: the street at exactly the pace of his planted foot (the
  // ground he's walked, rabbit.h), the near towers slower, the far ones slower still. When he
  // stops to talk, it all stops. (The lock screen's own scroll isn't used: it's the
  // sasquatch's pace, and his foot slides on it.)
  (void)scroll;
  const float ground = rabbit::walker().ground;
  neonTowers(d, t, ground * 0.12f, phase, 26, g - 6, 64, mix(t.bg, t.line, 0.75f), false, false, 3);
  neonTowers(d, t, ground * 0.3f, phase, 38, g - 2, 92, t.panel, true, unread, 11);
  // Rain: thin streaks slanting down, wrapping as they go.
  const uint16_t rain = mix(t.bg, t.blue, 0.5f);
  const int drops = 46 * W / 480;
  for (int i = 0; i < drops; i++) {
    const uint32_t h = hash(i * 7 + 5);
    const int fall = (int)(phase * (40 + (h >> 20) % 30));
    const int x = neonWrap((float)(h % W) - fall * 0.25f, W);
    const int y = 18 + neonWrap((float)((h >> 8) % (g - 18)) + fall, g - 18);
    d.drawLine(x, y, x - 1, y + 5, rain);
  }
  // The street: a neon kerb, lane marks streaming past under his feet at the ground's own
  // speed, and the glow of his visor on the wet road where he is.
  d.fillRect(0, g - 3, W, 3, mix(t.bg, t.line, 0.6f));
  for (int i = 0; i < W / 24 + 2; i++) {
    const int lx = neonWrap(i * 24 - ground, W + 24) - 24;
    d.drawFastHLine(lx, g - 2, 10, mix(t.bg, t.amber, 0.5f));
  }
  const uint16_t eye = unread ? t.amber : t.green;
  for (int k = 0; k < 3; k++) d.drawFastHLine(RABBIT_X - 14 + k * 4, g - 1 - k % 2, 28 - k * 8, mix(t.bg, eye, 0.18f));
  d.drawFastHLine(0, g, W, t.green);
  d.drawFastHLine(0, g + 1, W, mix(t.bg, t.green, 0.35f));
  // He's drawn before the towers' next frame needs his ground, so step him first: done
  // inside drawRabbitLock, which the street above read from the frame before - a frame
  // behind at most, at 4 px a frame.
  drawRabbitLock(d, t, RABBIT_X, g - mascotLift(), rabbitsprite::H, phase, eye, mascotPose());
}
