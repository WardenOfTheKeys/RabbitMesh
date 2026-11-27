// RabbitMesh's holiday lock scenes: the rainy-city street (scene_rabbit.h) dressed for the
// season, the rabbit walking it as ever. On five of them he's in costume: one complete sprite a
// frame from that holiday's set (holiday_sprite.h, from the owner's costume art via
// tools/make_holiday_frames.py and make_holiday_sprites.py), drawn in place of the base frames.
// The Halloween ghost is the owner's art too (costume_sprite.h), drawn on its own.
//
//   Halloween     a purple night, a huge moon, bats, fog, jack-o'-lanterns along the street;
//                 witch's hat and cape, and a little ghost floats along beside him
//   Thanksgiving  an amber dusk, a low harvest moon, leaves blowing across; pilgrim's hat and coat
//   Christmas     snow falling, lights strung over the street, neon trees; Santa's hat and coat
//   New Year's    fireworks over the skyline (gold, silver, magenta); party hat, gold-trimmed coat
//   4th of July   fireworks in red, white and blue; stars-and-stripes cap and jacket
//   Easter        a pastel dawn, glowing eggs along the street, flowers in the kerb (no costume)
// Included by scenes.h inside namespace scenes, after scene_rabbit.h (neonTowers, RABBIT_X);
// scenes.h includes costume_sprite.h at the top, outside the namespace.

// ---- shared ------------------------------------------------------------------------------------
// One costume item (costume_sprite.h, from the owner's art via tools/make_costumes.py), its
// anchor (bottom middle) at (x, y): runs of one colour, so plain line drawing.
inline void costumeAt(lgfx::LovyanGFX& d, const costume::Item& it, int x, int y) {
  const int ox = x - it.ax, oy = y - it.ay;
  for (int i = 0; i < it.nRuns; i++) {
    const uint8_t* r = it.runs + i * 4;
    d.drawFastHLine(ox + r[1], oy + r[0], r[2], it.pal[r[3]]);
  }
}
inline void holidayStreet(lgfx::LovyanGFX& d, const Theme& t, float ground, uint16_t kerb, uint16_t marks) {
  const int g = GROUND;
  d.fillRect(0, g - 3, W, 3, kerb);
  for (int i = 0; i < W / 24 + 2; i++) {
    const int lx = neonWrap(i * 24 - ground, W + 24) - 24;
    d.drawFastHLine(lx, g - 2, 10, marks);
  }
}
inline void holidayRabbit(lgfx::LovyanGFX& d, const Theme& t, float phase, bool unread) {
  const int g = GROUND;
  const uint16_t eye = unread ? t.amber : t.green;
  for (int k = 0; k < 3; k++) d.drawFastHLine(RABBIT_X - 14 + k * 4, g - 1 - k % 2, 28 - k * 8, mix(t.bg, eye, 0.18f));
  d.drawFastHLine(0, g, W, t.green);
  d.drawFastHLine(0, g + 1, W, mix(t.bg, t.green, 0.35f));
  drawRabbitLock(d, t, RABBIT_X, g - mascotLift(), rabbitsprite::H, phase, eye, mascotPose());
}
// Him in a holiday's costume: that set's frames for this one drawing, then the base ones again.
inline void costumeRabbit(lgfx::LovyanGFX& d, const Theme& t, float phase, bool unread, const holidaysprite::Set& cs) {
  rabbit::costumeSet() = &cs;
  holidayRabbit(d, t, phase, unread);
  rabbit::costumeSet() = nullptr;
}
// ---- Halloween -----------------------------------------------------------------------------------
inline void jackOLantern(lgfx::LovyanGFX& d, const Theme& t, int x, int y, float flick) {
  d.fillEllipse(x, y, 7, 5, t.green);                            // the pumpkin (the theme's orange)
  d.drawFastVLine(x, y - 7, 3, t.greenDim);                      // its stem
  const uint16_t glow = mix(t.amber, t.white, 0.2f + 0.3f * flick);
  d.fillTriangle(x - 4, y - 1, x - 2, y - 1, x - 3, y - 3, glow);
  d.fillTriangle(x + 2, y - 1, x + 4, y - 1, x + 3, y - 3, glow);
  d.drawFastHLine(x - 3, y + 2, 7, glow);
}
inline void rabbitHalloween(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread) {
  (void)scroll;
  const int g = GROUND;
  const float ground = rabbit::walker().ground;
  for (int k = 0; k < 6; k++) d.fillRect(0, g - 80 + k * 13, W, 13, mix(t.bg, t.greenDim, 0.03f + k * 0.03f));
  stars(d, t.dim, phase, 24, g - 80);
  {                                                              // the moon, huge and low
    const int mx = NARROW ? W - 70 : W - 110, my = g - 70, r = NARROW ? 26 : 32;
    const uint16_t moon = rgb(0xf4ecc8), crater = rgb(0xd9cfa3);   // a pale moon, whatever the palette
    d.fillCircle(mx, my, r + 3, mix(t.bg, moon, 0.18f));
    d.fillCircle(mx, my, r, moon);
    d.fillCircle(mx - 8, my - 5, 5, crater);
    d.fillCircle(mx + 9, my + 8, 4, crater);
  }
  for (int b = 0; b < 4; b++) {                                  // bats
    const uint32_t h = hash(b * 13 + 1);
    const int bx = neonWrap((float)(h % W) - phase * (14 + b * 3), W);
    const int by = 28 + (int)((h >> 8) % 40) + (int)(sinf(phase * 2 + b) * 4);
    const int flap = fmodf(phase * 6 + b, 2.0f) < 1.0f ? 3 : -2;
    d.drawLine(bx - 5, by - flap, bx, by, t.white); d.drawLine(bx, by, bx + 5, by - flap, t.white);
  }
  neonTowers(d, t, ground * 0.12f, phase, 26, g - 6, 60, mix(t.bg, t.line, 0.75f), false, false, 5);
  neonTowers(d, t, ground * 0.3f, phase, 38, g - 2, 88, t.panel, true, unread, 13);
  for (int k = 0; k < 3; k++) {                                  // fog drifting along the street
    const int fy = g - 14 + k * 4;
    for (int x = neonWrap(-phase * (6 + k * 3), 6); x < W; x += 6) d.drawFastHLine(x, fy, 3, mix(t.bg, t.white, 0.12f));
  }
  holidayStreet(d, t, ground, mix(t.bg, t.line, 0.6f), mix(t.bg, t.green, 0.5f));
  for (int i = 0; i < W / 80 + 2; i++) {                         // jack-o'-lanterns along the kerb
    const int x = neonWrap(i * 80 + 30 - ground, W + 80) - 40;
    jackOLantern(d, t, x, g - 9, 0.5f + 0.5f * sinf(phase * 9 + i * 2.1f));
  }
  costumeRabbit(d, t, phase, unread, holidaysprite::HALLOWEEN);
  costumeAt(d, costume::GHOST, RABBIT_X + 56, g - 30 + (int)(sinf(phase * 1.7f) * 5));   // floating along at his side
}

// ---- Thanksgiving --------------------------------------------------------------------------------
inline void rabbitThanksgiving(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread) {
  (void)scroll;
  const int g = GROUND;
  const float ground = rabbit::walker().ground;
  for (int k = 0; k < 7; k++) d.fillRect(0, g - 84 + k * 12, W, 12, mix(t.bg, t.green, 0.03f + k * 0.045f));   // an amber dusk
  {
    const int mx = NARROW ? W - 80 : W - 120, my = g - 46, r = NARROW ? 24 : 30;
    d.fillCircle(mx, my, r, mix(t.green, t.amber, 0.4f));        // the harvest moon, low and orange
  }
  neonTowers(d, t, ground * 0.12f, phase, 26, g - 6, 60, mix(t.bg, t.line, 0.75f), false, false, 7);
  neonTowers(d, t, ground * 0.3f, phase, 38, g - 2, 88, t.panel, true, unread, 17);
  const uint16_t leafC[3] = {t.green, t.red, t.amber};
  for (int i = 0; i < 26 * W / 320; i++) {                       // leaves blowing across and down
    const uint32_t h = hash(i * 29 + 3);
    const float sp = 20 + (h >> 20) % 25;
    const int x = neonWrap((float)(h % W) - phase * sp, W);
    const int y = 20 + neonWrap((float)((h >> 8) % (g - 30)) + phase * sp * 0.45f, g - 30);
    const bool tumble = fmodf(phase * 3 + i, 2.0f) < 1.0f;
    d.fillRect(x, y, tumble ? 3 : 2, tumble ? 2 : 3, leafC[(h >> 4) % 3]);
  }
  holidayStreet(d, t, ground, mix(t.bg, t.line, 0.6f), mix(t.bg, t.amber, 0.5f));
  costumeRabbit(d, t, phase, unread, holidaysprite::THANKSGIVING);
}

// ---- Christmas -------------------------------------------------------------------------------------
inline void neonTree(lgfx::LovyanGFX& d, const Theme& t, int x, int base, float phase, int seed) {
  for (int k = 0; k < 3; k++) {
    const int w = 16 - k * 4, y = base - 6 - k * 9;
    d.fillTriangle(x - w, y, x + w, y, x, y - 12, mix(t.bg, t.green, 0.55f));
    d.drawLine(x - w, y, x, y - 12, t.green); d.drawLine(x, y - 12, x + w, y, t.green);
  }
  d.fillRect(x - 2, base - 6, 4, 6, t.greenDim);
  const bool on = fmodf(phase * 1.5f + seed, 2.0f) < 1.4f;
  d.fillCircle(x, base - 37, 2, on ? t.amber : mix(t.bg, t.amber, 0.4f));   // the star
  for (int b = 0; b < 6; b++) {                                  // baubles
    const uint32_t h = hash(seed * 7 + b);
    const int bx = x - 10 + (int)(h % 20), by = base - 10 - (int)((h >> 8) % 24);
    if (abs(bx - x) < 14 - (base - by) / 3) d.drawPixel(bx, by, b % 2 ? t.red : t.amber);
  }
}
inline void rabbitChristmas(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread) {
  (void)scroll;
  const int g = GROUND;
  const float ground = rabbit::walker().ground;
  for (int k = 0; k < 6; k++) d.fillRect(0, g - 70 + k * 10, W, 10, mix(t.bg, t.blue, 0.03f + k * 0.03f));
  stars(d, t.dim, phase, 26, g - 80);
  neonTowers(d, t, ground * 0.12f, phase, 26, g - 6, 60, mix(t.bg, t.line, 0.75f), false, false, 9);
  neonTowers(d, t, ground * 0.3f, phase, 38, g - 2, 88, t.panel, true, unread, 19);
  {                                                              // lights strung across the street
    const float s = ground * 0.3f;
    for (int i = 0; i < W / 60 + 2; i++) {
      const int x0 = neonWrap(i * 60 - s, W + 60) - 60;
      for (int k = 0; k <= 12; k++) {
        const int x = x0 + k * 5, y = 30 + (int)(sinf(k * 3.1416f / 12) * 9);
        d.drawPixel(x, y, mix(t.bg, t.dim, 0.6f));
        if (k % 2 == 0) {
          const uint16_t c[3] = {t.red, t.green, t.amber};
          const bool on = fmodf(phase * 2 + k + i, 3.0f) < 2.2f;
          d.fillRect(x, y + 1, 2, 2, on ? c[(k / 2 + i) % 3] : mix(t.bg, c[(k / 2 + i) % 3], 0.3f));
        }
      }
    }
  }
  for (int i = 0; i < W / 140 + 2; i++) neonTree(d, t, neonWrap(i * 140 + 90 - ground * 0.6f, W + 140) - 70, g - 2, phase, i);
  for (int i = 0; i < 60 * W / 320; i++) {                       // snow
    const uint32_t h = hash(i * 11 + 7);
    const float sp = 10 + (h >> 20) % 14;
    const int x = neonWrap((float)(h % W) + sinf(phase + i) * 3 - phase * 3, W);
    const int y = 18 + neonWrap((float)((h >> 8) % (g - 18)) + phase * sp, g - 18);
    d.drawPixel(x, y, (h >> 4) % 4 ? t.white : mix(t.bg, t.white, 0.6f));
  }
  holidayStreet(d, t, ground, mix(t.bg, t.white, 0.75f), mix(t.bg, t.red, 0.6f));   // a snowy kerb
  costumeRabbit(d, t, phase, unread, holidaysprite::CHRISTMAS);
}

// ---- fireworks: New Year's and the 4th of July ------------------------------------------------------
// Each rocket climbs, bursts into a ring of sparks that fall and fade, then a new one goes up
// somewhere else. The colours are the theme's: gold/silver/magenta or red/white/blue.
inline void fireworks(lgfx::LovyanGFX& d, const Theme& t, float phase, int top, int bottom) {
  const uint16_t C[4] = {t.green, t.greenDim, t.white, t.amber};
  for (int r = 0; r < 5; r++) {
    const float cyc = 3.0f + r * 0.55f, f = fmodf(phase + r * 1.37f, cyc) / cyc;   // 0..1 through one rocket
    const int n = (int)((phase + r * 1.37f) / cyc);
    const uint32_t h = hash(r * 97 + n * 31);
    const int x = 20 + (int)(h % (W - 40)), burstY = top + (int)((h >> 8) % 40);
    const uint16_t c = C[(h >> 4) % 4];
    if (f < 0.3f) {                                              // climbing
      const int y = bottom - (int)((bottom - burstY) * (f / 0.3f));
      d.drawFastVLine(x, y, 4, mix(t.bg, t.amber, 0.7f));
      continue;
    }
    const float b = (f - 0.3f) / 0.7f;                           // bursting, falling, fading
    const int rad = (int)(5 + b * 28);
    const uint16_t sc = mix(t.bg, c, 1.0f - b * 0.75f);
    if (b < 0.12f) d.fillCircle(x, burstY, 4 - (int)(b * 25), t.white);   // the flash
    for (int k = 0; k < 20; k++) {
      const float a = k * 0.31416f + (h & 7);
      const int sx = x + (int)(cosf(a) * rad), sy = burstY + (int)(sinf(a) * rad) + (int)(b * b * 16);
      if (b < 0.45f) d.fillRect(sx, sy, 2, 2, sc); else d.drawPixel(sx, sy, sc);
      d.drawPixel(sx - (int)(cosf(a) * 3), sy - (int)(sinf(a) * 3), mix(t.bg, sc, 0.55f));   // a trail
    }
  }
}
inline void rabbitFireworks(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread, bool july) {
  (void)scroll;
  const int g = GROUND;
  const float ground = rabbit::walker().ground;
  for (int k = 0; k < 6; k++) d.fillRect(0, g - 70 + k * 10, W, 10, mix(t.bg, t.blue, 0.02f + k * 0.03f));
  stars(d, t.dim, phase, 22, g - 80);
  fireworks(d, t, phase, 26, g - 60);
  neonTowers(d, t, ground * 0.12f, phase, 26, g - 6, 60, mix(t.bg, t.line, 0.75f), false, false, july ? 23 : 21);
  neonTowers(d, t, ground * 0.3f, phase, 38, g - 2, 88, t.panel, true, unread, july ? 29 : 27);
  holidayStreet(d, t, ground, mix(t.bg, t.line, 0.6f), mix(t.bg, t.amber, 0.5f));
  costumeRabbit(d, t, phase, unread, july ? holidaysprite::JULY4 : holidaysprite::NEWYEAR);
}

// ---- Easter ----------------------------------------------------------------------------------------
inline void egg(lgfx::LovyanGFX& d, int x, int y, uint16_t c, uint16_t band, uint16_t glow) {
  d.fillEllipse(x, y, 5, 7, glow);
  d.fillEllipse(x, y, 4, 6, c);
  d.drawFastHLine(x - 3, y - 1, 7, band);
  d.drawFastHLine(x - 4, y + 2, 9, band);
}
inline void rabbitEaster(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread) {
  (void)scroll;
  const int g = GROUND;
  const float ground = rabbit::walker().ground;
  for (int k = 0; k < 7; k++) d.fillRect(0, g - 84 + k * 12, W, 12, mix(t.bg, k < 4 ? t.greenDim : t.green, 0.04f + k * 0.035f));   // a pastel dawn
  {
    const int sx = NARROW ? W - 90 : W - 130, sy = g - 30, r = NARROW ? 22 : 28;
    for (int y = -r; y <= 0; y++) d.drawFastHLine(sx - (int)sqrtf((float)(r * r - y * y)), sy + y, 2 * (int)sqrtf((float)(r * r - y * y)), mix(t.amber, t.white, 0.3f + 0.4f * (-y) / r));
  }
  neonTowers(d, t, ground * 0.12f, phase, 26, g - 6, 56, mix(t.bg, t.line, 0.7f), false, false, 31);
  neonTowers(d, t, ground * 0.3f, phase, 38, g - 2, 84, t.panel, true, unread, 37);
  holidayStreet(d, t, ground, mix(t.bg, t.green, 0.35f), mix(t.bg, t.white, 0.5f));
  const uint16_t eggC[4] = {t.green, t.greenDim, t.amber, t.blue};
  for (int i = 0; i < W / 46 + 2; i++) {                         // eggs and flowers along the kerb
    const int x = neonWrap(i * 46 + 20 - ground, W + 46) - 23;
    const uint32_t h = hash(i * 5 + 41);
    if (i % 3 == 1) {
      d.drawFastVLine(x, g - 9, 6, t.green);
      const uint16_t pc = eggC[(h >> 6) % 4];
      for (int k = 0; k < 4; k++) d.fillCircle(x + (k == 0 ? -2 : k == 1 ? 2 : 0), g - 11 + (k == 2 ? -2 : k == 3 ? 2 : 0), 1, pc);
      d.drawPixel(x, g - 11, t.amber);
    } else {
      const float pulse = 0.25f + 0.2f * sinf(phase * 2 + i);
      egg(d, x, g - 10, eggC[h % 4], mix(eggC[h % 4], t.white, 0.6f), mix(t.bg, eggC[h % 4], pulse));
    }
  }
  holidayRabbit(d, t, phase, unread);
}
