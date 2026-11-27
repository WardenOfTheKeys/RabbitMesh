// Squatch Mesh's sasquatch vs RabbitMesh's rabbit on the lock screen: the same frames, the
// same pace (home.cpp: phase +0.32, scroll +2 a frame), the same poses at the same
// moments. Measures the motion of each character on its own, then renders both lock
// screens for a side-by-side GIF. Preview/test only.
#include "themes.h"
#include "squatch_talk.h"
#include <cstdio>
#include <string>
#include <vector>
#include <set>
#include <cmath>
#include <cstdlib>
Nav nav;

static void setTheme(LGFX& d, int idx) { const ThemeSpec& s = THEMES[idx]; nav.th.apply(d, s.palette, s.style); nav.th.scene = s.scene; }
static bool rabbitTheme() { return nav.th.scene == SCENE_NEONRABBIT; }

// The character alone on the background, at the size the lock screen draws it.
static void drawChar(LGFX& d, float phase, const SquatchPose& p) {
  const Theme& t = nav.th;
  d.fillRect(0, 0, 320, 240, t.bg);
  if (rabbitTheme()) drawRabbit(d, t, 160, 200, rabbitsprite::H, phase, t.green, p);
  else drawSasquatch(d, t, 160, 200, 88, phase, t.green, p);
}

struct Box { int x0 = 9999, y0 = 9999, x1 = -1, y1 = -1; };
static Box bbox(const LGFX& d, uint16_t bg) {
  Box b;
  for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) if (d.px[y * 320 + x] != bg) {
    if (x < b.x0) b.x0 = x; if (x > b.x1) b.x1 = x; if (y < b.y0) b.y0 = y; if (y > b.y1) b.y1 = y; }
  return b;
}
// The spread of the feet: the opaque columns in the lowest 6 rows of the figure.
static int feetSpan(const LGFX& d, uint16_t bg, int bottom) {
  int lo = 9999, hi = -1;
  for (int y = bottom - 5; y <= bottom; y++) for (int x = 0; x < 320; x++) if (d.px[y * 320 + x] != bg) { if (x < lo) lo = x; if (x > hi) hi = x; }
  return hi - lo + 1;
}

static void measure(LGFX& d, const char* name) {
  const uint16_t bg = nav.th.bg;
  const int N = (int)ceilf(2 * (float)M_PI / 0.32f);           // frames in one stride
  std::vector<uint16_t> prev;
  long changedSum = 0, changedMax = 0, still = 0, callsSum = 0, pixSum = 0;
  int topMin = 999, topMax = -1, botMin = 999, botMax = -1, feetMin = 999, feetMax = -1, wMin = 999, wMax = -1;
  std::set<std::vector<uint16_t>> distinct;
  for (int f = 0; f <= N; f++) {
    lgfx::stats() = lgfx::DrawStats();
    drawChar(d, f * 0.32f, SquatchPose());
    const long calls = lgfx::stats().calls - 1, pix = lgfx::stats().pixels - 320 * 240;   // minus the clear
    const Box b = bbox(d, bg);
    if (f < N) {
      callsSum += calls; pixSum += pix; distinct.insert(d.px);
      topMin = std::min(topMin, b.y0); topMax = std::max(topMax, b.y0);
      botMin = std::min(botMin, b.y1); botMax = std::max(botMax, b.y1);
      const int fs = feetSpan(d, bg, b.y1); feetMin = std::min(feetMin, fs); feetMax = std::max(feetMax, fs);
      wMin = std::min(wMin, b.x1 - b.x0 + 1); wMax = std::max(wMax, b.x1 - b.x0 + 1);
    }
    if (getenv("PERFRAME") && f < N) printf("  %s f%02d top %d bottom %d width %d\n", name, f, b.y0, b.y1, b.x1 - b.x0 + 1);
    if (!prev.empty()) {
      long c = 0; for (size_t i = 0; i < prev.size(); i++) if (prev[i] != d.px[i]) c++;
      if (getenv("PERFRAME")) printf("  %s f%02d changed %ld\n", name, f, c);
      changedSum += c; changedMax = std::max(changedMax, c); if (c == 0) still++;
    }
    prev = d.px;
  }
  const Box b = bbox(d, bg);
  printf("%-10s size %3dx%3d px | stride: %d frames, %zu distinct | bob (top) %d px, feet lift (bottom) %d px | "
         "feet spread %d..%d px, width %d..%d | changed px/frame avg %ld max %ld, still frames %ld | draw calls %ld, px written %ld\n",
         name, b.x1 - b.x0 + 1, b.y1 - b.y0 + 1, N, distinct.size(), topMax - topMin, botMax - botMin, feetMin, feetMax, wMin, wMax,
         changedSum / N, changedMax, still, callsSum / N, pixSum / N);
}

static void save(LGFX& d, const std::string& stem) {
  FILE* f = fopen((stem + ".ppm").c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
  f = fopen((stem + ".txt").c_str(), "w");
  for (auto& o : textOps()) fprintf(f, "%d\t%d\t%d\t%u\t%s\n", o.x, o.y, o.font, o.color, o.text.c_str());
  fclose(f); textOps().clear();
}

// One lock frame as LockView draws it, for whichever theme is set.
static long lockFrame(LGFX& d, int f, const std::string& stem) {
  const Theme& t = nav.th;
  SquatchPose p; const char* say = nullptr; const bool unread = f >= 32;
  if (f >= 14 && f < 32) { p.mouth = (f / 2) % 2; say = "Hey there!"; }
  if (f >= 32) { p.wave = 1; say = "A message came in!"; }
  lgfx::stats() = lgfx::DrawStats();
  d.fillRect(0, 0, 320, 240, t.bg);
  d.fillRect(0, 0, 320, 18, t.panel);
  scenes::mascotPose() = p;
  const float phase = f * 0.32f, scroll = f * 2.0f;
  if (rabbitTheme()) scenes::neonRabbit(d, t, phase, scroll, unread); else scenes::inw(d, t, phase, scroll, unread);
  scenes::mascotPose() = SquatchPose();
  if (say) { int ax, ay; talk::anchor(t.style, 0, ax, ay); talk::bubble(d, t, ax, ay, say, 1.0f); }
  d.fillRect(0, scenes::GROUND + 2, 320, 240 - scenes::GROUND - 2, t.bg);
  d.setFont(&fonts::Font6); d.setTextColor(t.green); d.drawString("9:41", 105, scenes::GROUND + 9);
  d.setFont(&fonts::Font2); d.setTextColor(t.dim); d.drawString(rabbitTheme() ? "RabbitMesh" : "Squatch Mesh", 6, 222);
  const long calls = lgfx::stats().calls;
  save(d, stem);
  return calls;
}

int main() {
  LGFX d;
  printf("one stride, character alone, drawn at its lock-screen size:\n");
  setTheme(d, 0); measure(d, "sasquatch");
  setTheme(d, THEME_COUNT - 1); measure(d, "rabbit");
  long cs = 0, cr = 0;
  char stem[32];
  for (int f = 0; f < 40; f++) {
    setTheme(d, 0); snprintf(stem, sizeof stem, "sq_%02d", f); cs += lockFrame(d, f, stem);
    setTheme(d, THEME_COUNT - 1); snprintf(stem, sizeof stem, "rb_%02d", f); cr += lockFrame(d, f, stem);
  }
  printf("whole lock frame (scene + character + bubble), draw calls per frame: squatch %ld, rabbit %ld\n", cs / 40, cr / 40);
}
