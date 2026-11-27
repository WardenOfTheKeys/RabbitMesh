// Planted foot and lean, the rabbit against the sasquatch, frame by frame while walking.
//   slip: how far the foot carrying his weight moves against the ground in one frame (0 =
//         planted; the ground is what the scene scrolls under him: the sasquatch's 2 px a
//         frame, the rabbit's own stride)
//   lean: how far the front of his head is ahead of the front of his hip
#include "themes.h"
#include "squatch_talk.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstdlib>
Nav nav;
struct Run { int a, b; };
static std::vector<Run> ground(const LGFX& d, uint16_t bg, int& bottom, int& top, int& lean) {
  bottom = -1; top = 999;
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) if (d.px[y * 320 + x] != bg) { if (y > bottom) bottom = y; if (y < top) top = y; }
  std::vector<Run> r; int s = -1;
  for (int x = 0; x <= 320; x++) { const bool v = x < 320 && d.px[(bottom - 1) * 320 + x] != bg;
    if (v && s < 0) s = x; if (!v && s >= 0) { if (x - s >= 3) r.push_back({s, x - 1}); s = -1; } }
  int head = -1, hip = -1; const int hy = top + (bottom - top) * 62 / 100;
  for (int y = top; y < top + 15; y++) for (int x = 0; x < 320; x++) if (d.px[y * 320 + x] != bg && x > head) head = x;
  for (int y = hy - 2; y <= hy + 2; y++) for (int x = 0; x < 320; x++) if (d.px[y * 320 + x] != bg && x > hip) hip = x;
  lean = head - hip;
  return r;
}
int main() {
  LGFX d;
  for (int th : {0, (int)THEME_COUNT - 1}) {
    const ThemeSpec& s = THEMES[th]; nav.th.apply(d, s.palette, s.style); nav.th.scene = s.scene; const Theme& t = nav.th;
    float sum = 0, mx = 0; int n = 0;
    std::vector<Run> prev; float prevG = 0;
    printf("%s\n  slip px: ", th ? "rabbit   " : "sasquatch");
    std::vector<int> leans;
    for (int f = 0; f < 40; f++) {
      const float ph = f * 0.32f;
      d.fillRect(0, 0, 320, 240, t.bg);
      float g;
      if (th) { drawRabbitShot(d, t, 160, 200, rabbitsprite::H, ph, t.green, SquatchPose(), rabbit::walkShot(ph, 0)); g = rabbit::groundAt(ph); }
      else { drawSasquatch(d, t, 160, 200, 88, ph, t.green, SquatchPose()); g = f * 2.0f; }
      int bottom, top, lean;
      std::vector<Run> r = ground(d, t.bg, bottom, top, lean);
      leans.push_back(lean);
      if (th && getenv("DETAIL")) { const rabbit::Shot sh = rabbit::walkShot(ph, 0); fprintf(stderr, "f%02d drawing %d sub %.2f g %.1f runs", f, sh.frame, sh.sub, g); for (auto& b : r) fprintf(stderr, " [%d-%d]=%.0f", b.a, b.b, b.a + g); fprintf(stderr, "\n"); }
      if (th && getenv("STANCE")) {
        // the rabbit: the boot the converter says carries him, where the firmware put it
        const rabbit::Shot sh = rabbit::walkShot(ph, 0);
        const int x0 = rabbitsprite::STANCE_X0[sh.frame], x1 = rabbitsprite::STANCE_X1[sh.frame];
        const int left = 160 - rabbitsprite::FOOT_X;
        int back = -1;
        for (auto& b : r) { const int a = b.a - left, e = b.b - left; if (e >= x0 - 25 && a <= x1 + 2) { back = b.a; break; } }
        static int pFrame = -1; static float pPos = 0;
        const bool sameFoot = pFrame == sh.frame || (pFrame >= 0 && (pFrame + 1) % rabbitsprite::NWALK == sh.frame && sh.frame % (rabbitsprite::NWALK / 2) != 0);
        if (back >= 0) {
          const float pos = back + g;
          if (sameFoot) { const float m = fabsf(pos - pPos); printf("%.0f ", m); sum += m; if (m > mx) mx = m; n++; }
          else printf("| ");
          pPos = pos; pFrame = sh.frame;
        }
      } else if (!prev.empty()) {
        float best = 1e9;
        for (auto& a : prev) for (auto& b : r) {
          const float m = (b.a + g) - (a.a + prevG);
          if (fabsf(m) < fabsf(best)) best = m;
        }
        // The rabbit leaving a contact drawing (both feet down) puts his weight on the other
        // foot: that's the weight transfer, not a slip, so it isn't counted.
        static int pFr = -1;
        const int fr = th ? rabbit::walkShot(ph, 0).frame : -1;
        const bool transfer = th && pFr >= 0 && fr != pFr && pFr % (rabbitsprite::NWALK / 2) == 0;
        pFr = fr;
        if (transfer) { printf("| "); best = 1e9; }
        if (best < 1e8) { printf("%.0f ", fabsf(best)); sum += fabsf(best); if (fabsf(best) > mx) mx = fabsf(best); n++; }
      }
      prev = r; prevG = g;
    }
    printf("\n  average slip %.1f px a frame, worst %.0f\n  lean px: ", n ? sum / n : 0, mx);
    for (int l : leans) printf("%d ", l);
    printf("\n");
  }
}
