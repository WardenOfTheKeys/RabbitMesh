// Both characters alone on their themes' backgrounds, the same 40 frames and poses as
// the lock-screen comparison, for a close-up side by side. Writes cu_sq_NN / cu_rb_NN.ppm
// and guides.txt: each character's highest head row, and its ground row, measured.
#include "themes.h"
#include "squatch_talk.h"
#include <cstdio>
#include <string>
Nav nav;
static void save(LGFX& d, const std::string& n) {
  FILE* f = fopen(n.c_str(), "wb"); fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
int main() {
  LGFX d;
  FILE* g = fopen("guides.txt", "w");
  for (int th : {0, (int)THEME_COUNT - 1}) {
    const ThemeSpec& s = THEMES[th]; nav.th.apply(d, s.palette, s.style); nav.th.scene = s.scene; const Theme& t = nav.th;
    int topMin = 999, bottomMax = -1;
    for (int f = 0; f < 40; f++) {
      SquatchPose p;
      if (f >= 14 && f < 32) p.mouth = (f / 2) % 2;
      if (f >= 32) p.wave = 1;
      d.fillRect(0, 0, 320, 240, t.bg);
      if (th) drawRabbit(d, t, 70, 200, rabbitsprite::H, f * 0.32f, f >= 32 ? t.amber : t.green, p);
      else drawSasquatch(d, t, 70, 200, 88, f * 0.32f, f >= 32 ? t.amber : t.green, p);
      // the walk only: a wave raises an arm higher than his head
      if (f < 14) for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) if (d.px[y * 320 + x] != t.bg) { if (y < topMin) topMin = y; if (y > bottomMax) bottomMax = y; }
      char n[32]; snprintf(n, sizeof n, "cu_%s_%02d.ppm", th ? "rb" : "sq", f); save(d, n);
    }
    fprintf(g, "%s %d %d\n", th ? "rb" : "sq", topMin, bottomMax);
  }
  fclose(g);
}
