// One stride of each character, six steps side by side, on its own theme's background.
#include "themes.h"
#include "squatch_talk.h"
#include <cstdio>
Nav nav;
static void save(LGFX& d, const char* n) {
  FILE* f = fopen(n, "wb"); fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
}
int main() {
  LGFX d;
  for (int th : {0, (int)THEME_COUNT - 1}) {
    const ThemeSpec& s = THEMES[th]; nav.th.apply(d, s.palette, s.style); nav.th.scene = s.scene; const Theme& t = nav.th;
    d.fillRect(0, 0, 320, 240, t.bg);
    for (int i = 0; i < 6; i++) {
      const float ph = i * 1.047f;
      d.drawFastHLine(i * 53 + 2, 200, 50, t.greenDim);
      if (th) drawRabbit(d, t, i * 53 + 27, 200, rabbitsprite::H, ph, t.green, SquatchPose());
      else drawSasquatch(d, t, i * 53 + 22, 200, 88, ph, t.green, SquatchPose());
    }
    save(d, th ? "strip_rabbit.ppm" : "strip_squatch.ppm");
  }
}
