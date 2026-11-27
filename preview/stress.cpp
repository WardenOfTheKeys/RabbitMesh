// Stress test for the rabbit puppet: every pose, many phases, sizes and positions (half
// off the canvas included, as in the screen changes), under ASan/UBSan. Also checks he
// never draws outside the box the rig allows, and that the canvas outside it is untouched.
#include "themes.h"
#include "squatch_talk.h"
#include <cstdio>
Nav nav;
int main() {
  LGFX d;
  const ThemeSpec& s = THEMES[THEME_COUNT - 1];
  nav.th.apply(d, s.palette, s.style); nav.th.scene = s.scene;
  const Theme& t = nav.th;
  long frames = 0, outside = 0;
  for (int pose = 0; pose < 9; pose++) {
    SquatchPose p;
    switch (pose) { case 1: p.blink = true; break; case 2: p.mouth = 1; break; case 3: p.mouth = 2; break; case 4: p.wave = 1; break;
      case 5: p.armsUp = 1; break; case 6: p.slump = 1; break; case 7: p.happy = true; break; case 8: p.dizzy = 1.3f; break; }
    for (int h = 10; h <= 200; h += 19)
      for (float ph = 0; ph < 30; ph += 2.53f)
        for (int x : {-60, 160, 380})
          for (int g : {-20, 152, 300}) {
            const uint16_t bg = 0x1234;
            d.fillRect(0, 0, 320, 240, bg);
            drawRabbit(d, t, x, g, h, ph, t.green, p);
            drawThemeMascot(d, t, x, g, h * 88 / rabbitsprite::H, ph, t.amber, p);
            frames++;
            // Nothing beyond the sprite's own box plus the rig's margins (and the stars/rings).
            const int dw = (rabbitsprite::W * h + rabbitsprite::H / 2) / rabbitsprite::H;
            const int mx = (16 * dw) / rabbitsprite::W + 2 + 20, my = (14 * h) / rabbitsprite::H + 2 + 20;
            const int lx = x - (rabbitsprite::FOOT_X * dw) / rabbitsprite::W - mx, rx = lx + dw + 2 * mx;
            const int ty = g - h + 1 - my, by = g + my;
            for (int yy = 0; yy < 240; yy++) for (int xx = 0; xx < 320; xx++)
              if (d.px[yy * 320 + xx] != bg && (xx < lx || xx > rx || yy < ty || yy > by)) outside++;
          }
  }
  printf("%ld frames drawn, %ld pixels outside the allowed box\n", frames, outside);
  return outside != 0;
}
