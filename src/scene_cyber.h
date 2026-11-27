// RabbitMesh's two cybersecurity themes' lock scenes, with the rabbit walking through them
// the way he walks the rainy city (scene_rabbit.h): he steps, the room goes by at the pace of
// his planted foot, and when he stops to talk it all stops.
//
//   Red Team:  a data centre after hours. Rows of racks slide past, status lights blinking,
//              cable trays overhead, a perforated raised floor; a red beacon turns on the
//              ceiling, and with something unread one rack's alarm strobes.
//   Blue Team: a security operations centre. A wall of monitors slides past behind him: logs
//              scrolling, traffic graphs, a radar sweep, attack arcs over a dotted map; with
//              something unread one screen flashes ALERT amber.
//
// Included by scenes.h inside namespace scenes, after scene_rabbit.h (RABBIT_X, neonWrap,
// rabbitAnchor: the speech bubble sits where it does in the city).

// ---- Red Team -------------------------------------------------------------------------------

// One rack: a frame, units every 6 px, each with a light or two and some drive bays.
inline void cyberRack(lgfx::LovyanGFX& d, const Theme& t, int x, int top, int w, int base, float phase,
                      uint32_t h, bool near, bool alarm) {
  const uint16_t frame = near ? mix(t.bg, t.line, 0.9f) : mix(t.bg, t.line, 0.45f);
  const uint16_t face = near ? mix(t.bg, t.panel, 0.9f) : mix(t.bg, t.panel, 0.5f);
  d.fillRect(x, top, w, base - top, face);
  d.drawRect(x, top, w, base - top, frame);
  const int units = (base - top - 4) / 6;
  for (int u = 0; u < units; u++) {
    const int uy = top + 3 + u * 6;
    const uint32_t uh = hash((int32_t)(h + u * 977));
    if (near) d.drawFastHLine(x + 2, uy + 5, w - 4, mix(face, frame, 0.5f));
    // drive bays on some units
    if ((uh & 3) == 0) for (int k = 0; k < (w - 14) / 3; k++) d.drawFastVLine(x + 10 + k * 3, uy + 1, 3, mix(face, t.dim, 0.35f));
    // status lights: steady, blinking (activity), or amber (a warning)
    const uint16_t lit[3] = {t.green, t.blue, t.amber};
    const uint16_t c = lit[(uh >> 4) % 7 == 0 ? 2 : (uh >> 4) % 3 == 0 ? 1 : 0];
    const bool blink = (uh >> 8) & 1;
    const bool on = !blink || fmodf(phase * (2.0f + (uh >> 12) % 5) + (uh & 15), 2.0f) < 1.0f;
    if (on) d.fillRect(x + 3, uy + 1, 2, 2, near ? c : mix(t.bg, c, 0.5f));
    if ((uh >> 9) & 1 && near) d.drawPixel(x + 6, uy + 2, on ? t.white : face);
  }
  if (alarm && fmodf(phase, 0.8f) < 0.4f) {
    d.fillRect(x + 1, top - 4, w - 2, 3, t.red);
    d.drawRect(x, top, w, base - top, t.red);
  }
}

inline void redTeam(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread) {
  (void)scroll;                                       // the street is his stride, as in the city
  const int g = GROUND;
  const float ground = rabbit::walker().ground;
  // Back wall, a little lighter near the floor.
  for (int k = 0; k < 5; k++) d.fillRect(0, g - 100 + k * 20, W, 20, mix(t.bg, t.panel, 0.15f + k * 0.08f));
  // Cable tray overhead, sliding with the far racks; cables droop between its hangers.
  const float trayS = ground * 0.5f;
  d.fillRect(0, 24, W, 3, mix(t.bg, t.line, 0.8f));
  for (int i = 0; i < W / 40 + 2; i++) {
    const int hx = neonWrap(i * 40 - trayS, W + 40) - 20;
    d.drawFastVLine(hx, 18, 6, mix(t.bg, t.line, 0.8f));
    for (int c = 0; c < 3; c++) {
      const uint16_t cc = mix(t.bg, c == 0 ? t.greenDim : c == 1 ? t.blue : t.dim, 0.5f);
      for (int k = 0; k < 40; k += 2) d.drawPixel(hx + k, 28 + c * 2 + (int)(sinf(k * 3.1416f / 40) * (3 + c)), cc);
    }
  }
  // The ceiling beacon: it turns, throwing a red beam across the room, brighter with something unread.
  {
    const int bx = NARROW ? W - 70 : W - 120, by = 36;
    const float a = phase * 1.6f;
    const float reach = (float)(g - by);
    const uint16_t beam = mix(t.bg, t.red, unread ? 0.32f : 0.14f);
    for (int k = -3; k <= 3; k++) {
      const float q = a + k * 0.05f;
      const int ex = bx + (int)(sinf(q) * reach * 1.4f), ey = by + (int)(fabsf(cosf(q)) * reach);
      d.drawLine(bx, by, ex, ey, beam);
    }
    d.fillRect(bx - 4, by - 6, 9, 5, mix(t.bg, t.line, 0.9f));
    d.fillCircle(bx, by, 3, fmodf(phase, 1.2f) < 0.6f ? t.red : mix(t.bg, t.red, 0.5f));
  }
  // Far racks, then near ones, at their own speeds.
  {
    const int pitch = 30, n = W / pitch + 3, span = n * pitch;
    for (int i = 0; i < n; i++) {
      const int x = neonWrap(i * pitch - ground * 0.25f, span) - pitch;
      const uint32_t h = hash(i * 53 + 7);
      cyberRack(d, t, x, g - 70 - (int)(h % 12), pitch - 6, g - 6, phase, h, false, false);
    }
  }
  {
    const int pitch = 50, n = W / pitch + 3, span = n * pitch;
    for (int i = 0; i < n; i++) {
      const int x = neonWrap(i * pitch - ground * 0.6f, span) - pitch;
      const uint32_t h = hash(i * 131 + 29);
      cyberRack(d, t, x, g - 98 - (int)(h % 10), pitch - 10, g - 2, phase, h, true, unread && i % 4 == 2);
    }
  }
  // The raised floor: tile seams streaming past at his foot's pace, a few perforated tiles.
  d.fillRect(0, g - 2, W, 2, mix(t.bg, t.line, 0.7f));
  for (int i = 0; i < W / 20 + 2; i++) {
    const int lx = neonWrap(i * 20 - ground, W + 20) - 20;
    d.drawFastVLine(lx, g - 2, 2, mix(t.bg, t.dim, 0.5f));
    if (hash(i * 17) % 3 == 0) for (int k = 2; k < 18; k += 3) d.drawPixel(lx + k, g - 1, mix(t.bg, t.dim, 0.4f));
  }
  const uint16_t eye = unread ? t.amber : t.green;
  for (int k = 0; k < 3; k++) d.drawFastHLine(RABBIT_X - 14 + k * 4, g - 1 - k % 2, 28 - k * 8, mix(t.bg, eye, 0.18f));
  d.drawFastHLine(0, g, W, t.green);
  d.drawFastHLine(0, g + 1, W, mix(t.bg, t.green, 0.35f));
  drawRabbitLock(d, t, RABBIT_X, g - mascotLift(), rabbitsprite::H, phase, eye, mascotPose());
}

// ---- Blue Team ------------------------------------------------------------------------------

// One monitor on the wall, showing one of four things.
inline void socScreen(lgfx::LovyanGFX& d, const Theme& t, int x, int y, int w, int h, float phase,
                      uint32_t hs, bool alert) {
  const uint16_t bezel = mix(t.bg, t.line, 0.9f), glass = mix(t.bg, t.panel, 0.8f);
  d.fillRect(x, y, w, h, glass);
  d.drawRect(x, y, w, h, bezel);
  if (alert) {                                            // something unread: ALERT, flashing
    const bool on = fmodf(phase, 1.0f) < 0.6f;
    d.fillRect(x + 2, y + 2, w - 4, h - 4, on ? mix(t.bg, t.amber, 0.35f) : glass);
    d.drawRect(x + 4, y + 4, w - 8, h - 8, t.amber);
    d.fillRect(x + w / 2 - 1, y + 7, 3, h - 18, t.amber);   // a "!"
    d.fillRect(x + w / 2 - 1, y + h - 9, 3, 3, t.amber);
    return;
  }
  const int ix = x + 3, iy = y + 3, iw = w - 6, ih = h - 6;
  switch ((hs >> 11) % 4) {
    case 0: {                                             // a log scrolling up
      const int rows = ih / 4, off = (int)(phase * 4);
      for (int r = 0; r < rows; r++) {
        const uint32_t lh = hash((int32_t)(hs + r + off));
        const int len = 6 + (int)(lh % (iw - 6));
        d.drawFastHLine(ix, iy + r * 4, len, (lh >> 8) % 9 == 0 ? t.red : (lh >> 8) % 3 == 0 ? t.green : mix(t.bg, t.txt, 0.45f));
      }
      break;
    }
    case 1: {                                             // a traffic graph, bars rising and falling
      const int bars = iw / 4;
      for (int b = 0; b < bars; b++) {
        const float v = 0.5f + 0.5f * sinf(phase * 1.7f + b * 0.9f + (hs & 7));
        const int bh = 2 + (int)(v * (ih - 2));
        d.fillRect(ix + b * 4, iy + ih - bh, 3, bh, v > 0.85f ? t.amber : t.green);
      }
      break;
    }
    case 2: {                                             // a radar sweep
      const int cx = ix + iw / 2, cy = iy + ih / 2, r = min(iw, ih) / 2 - 1;
      d.drawCircle(cx, cy, r, mix(t.bg, t.green, 0.4f));
      d.drawCircle(cx, cy, r / 2, mix(t.bg, t.green, 0.25f));
      const float a = phase * 2.2f + (hs & 7);
      d.drawLine(cx, cy, cx + (int)(cosf(a) * r), cy + (int)(sinf(a) * r), t.green);
      const float ba = (hs >> 4) % 6;                     // a blip that lights as the sweep passes
      if (fmodf(a - ba + 62.83f, 6.283f) < 1.2f) d.fillRect(cx + (int)(cosf(ba) * r * 0.6f), cy + (int)(sinf(ba) * r * 0.6f), 2, 2, t.white);
      break;
    }
    default: {                                            // a dotted map with an attack arc going over
      for (int yy = iy + 1; yy < iy + ih; yy += 3)
        for (int xx = ix + 1; xx < ix + iw; xx += 3)
          if (hash(xx * 7 + yy * 131 + (int32_t)hs) % 3 == 0) d.drawPixel(xx, yy, mix(t.bg, t.blue, 0.4f));
      const float f = fmodf(phase * 0.5f + (hs & 3) * 0.25f, 1.0f);
      const int x0 = ix + 2, x1 = ix + iw - 3, yb = iy + ih - 3;
      int px = x0, py = yb;
      for (int s = 1; s <= (int)(f * 12); s++) {
        const float u = s / 12.0f;
        const int qx = x0 + (int)((x1 - x0) * u), qy = yb - (int)(sinf(u * 3.1416f) * (ih - 5));
        d.drawLine(px, py, qx, qy, t.red);
        px = qx; py = qy;
      }
      d.fillRect(px - 1, py - 1, 3, 3, t.amber);
      break;
    }
  }
}

inline void blueTeam(lgfx::LovyanGFX& d, const Theme& t, float phase, float scroll, bool unread) {
  (void)scroll;
  const int g = GROUND;
  const float ground = rabbit::walker().ground;
  // The room: dark, the wall's glow brightest behind the screens.
  for (int k = 0; k < 6; k++) d.fillRect(0, 20 + k * 22, W, 22, mix(t.bg, t.panel, 0.1f + k * 0.05f));
  // The video wall: two rows of monitors sliding past slowly behind him.
  {
    const int pitch = 56, sw = 50, sh = 34, n = W / pitch + 3, span = n * pitch;
    for (int i = 0; i < n; i++) {
      const int x = neonWrap(i * pitch - ground * 0.3f, span) - pitch;
      for (int r = 0; r < 2; r++) {
        const uint32_t hs = hash(i * 41 + r * 7 + 3);
        socScreen(d, t, x, 30 + r * (sh + 6), sw, sh, phase, hs, unread && r == 0 && i % 5 == 2);
      }
    }
  }
  // The analysts' desks in front of the wall, nearer and quicker, each with its own little screens.
  {
    const int pitch = 90, n = W / pitch + 3, span = n * pitch;
    const uint16_t desk = mix(t.bg, t.line, 0.85f);
    for (int i = 0; i < n; i++) {
      const int x = neonWrap(i * pitch - ground * 0.65f, span) - pitch;
      d.fillRect(x, g - 22, 70, 4, desk);                 // the desk top
      d.fillRect(x + 4, g - 18, 3, 16, desk);             // its legs
      d.fillRect(x + 63, g - 18, 3, 16, desk);
      for (int k = 0; k < 2; k++) {                       // two monitors on it
        const int mx = x + 12 + k * 26;
        d.fillRect(mx, g - 40, 22, 15, mix(t.bg, t.panel, 0.9f));
        d.drawRect(mx, g - 40, 22, 15, desk);
        d.drawFastVLine(mx + 11, g - 25, 3, desk);
        const uint32_t hs = hash(i * 19 + k);
        for (int r = 0; r < 3; r++) {
          const int len = 4 + (int)(hash((int32_t)(hs + r + (int)(phase * 2))) % 14);
          d.drawFastHLine(mx + 2, g - 37 + r * 4, len, r == 0 ? t.green : mix(t.bg, t.txt, 0.4f));
        }
      }
    }
  }
  // The floor: tiles streaming under his feet, the screens' blue reflected on it.
  d.fillRect(0, g - 2, W, 2, mix(t.bg, t.blue, 0.18f));
  for (int i = 0; i < W / 24 + 2; i++) {
    const int lx = neonWrap(i * 24 - ground, W + 24) - 24;
    d.drawFastHLine(lx, g - 1, 12, mix(t.bg, t.blue, 0.35f));
  }
  const uint16_t eye = unread ? t.amber : t.green;
  for (int k = 0; k < 3; k++) d.drawFastHLine(RABBIT_X - 14 + k * 4, g - 1 - k % 2, 28 - k * 8, mix(t.bg, eye, 0.18f));
  d.drawFastHLine(0, g, W, t.green);
  d.drawFastHLine(0, g + 1, W, mix(t.bg, t.green, 0.35f));
  drawRabbitLock(d, t, RABBIT_X, g - mascotLift(), rabbitsprite::H, phase, eye, mascotPose());
}
