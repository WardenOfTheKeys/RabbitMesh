// RabbitMesh: walk -> talk -> walk on the lock screen, and a QA sheet of the talk frames.
//   qa_talk_K.ppm  every talk frame at 44 x 124 over a bright checkerboard; a transparent
//                  pixel walled in by his body (a hole the city would show through) is red
//   seq_NNN.ppm    the lock screen, frame by frame: walking, a line comes in (he walks on to
//                  a passing step, stops, raises the radio, talks, lowers it), walking again
// Preview/test only.
#include "themes.h"
#include "squatch_talk.h"
#include <cstdio>
#include <string>
#include <vector>
Nav nav;
static void save(LGFX& d, const std::string& n, int x0 = 0, int y0 = 0, int w = 320, int h = 240) {
  FILE* f = fopen(n.c_str(), "wb"); fprintf(f, "P6 %d %d 255\n", w, h);
  for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) {
    const uint16_t c = d.px[y * 320 + x];
    unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}
static void saveText(const std::string& n) {
  FILE* f = fopen(n.c_str(), "w");
  for (auto& o : textOps()) fprintf(f, "%d\t%d\t%d\t%u\t%s\n", o.x, o.y, o.font, o.color, o.text.c_str());
  fclose(f); textOps().clear();
}

int main() {
  LGFX d;
  const ThemeSpec& s = THEMES[THEME_COUNT - 1];
  nav.th.apply(d, s.palette, s.style); nav.th.scene = s.scene;
  const Theme& t = nav.th;
  using namespace rabbitsprite;

  // ---- QA: each talk frame at 44 x 124 on a checkerboard ---------------------------------------
  const uint16_t KEYC = 0xF81F;                         // a magenta no palette entry is
  FILE* rep = fopen("qa_report.txt", "w");
  int qn = 0;
  for (int pass = 0; pass < 2; pass++)
  for (int k = 0; k < NTALK; k++) for (int mouth = 0; mouth < (k == NTALK - 1 ? 2 : 1); mouth++) {
    d.fillRect(0, 0, 320, 240, KEYC);
    rabbit::Shot sh = {NWALK + k, 0, 0, 0, 0, mouth == 1, false};
    SquatchPose p; p.talking = true; if (mouth) p.mouth = 1;
    drawRabbitShot(d, t, 160, 200, H, 1.0f, t.green, p, sh);
    int x0 = 999, x1 = -1, y0 = 999, y1 = -1;
    for (int y = 0; y < 240; y++) for (int x = 0; x < 320; x++) if (d.px[y * 320 + x] != KEYC) {
      if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
    const int cw = pass ? (x1 - x0 + 1) + 4 : 44, ch = 124, cx0 = (x0 + x1 + 1) / 2 - cw / 2, cy0 = y1 - ch + 1;   // pass 0: 44 wide as asked; pass 1: the figure's own width
    // holes: transparent pixels inside the 44 x 124 box not reachable from its edge
    std::vector<int> seen(cw * ch, 0), st;
    auto key = [&](int x, int y) { return d.px[(cy0 + y) * 320 + cx0 + x] == KEYC; };
    for (int x = 0; x < cw; x++) for (int y : {0, ch - 1}) if (key(x, y)) { seen[y * cw + x] = 1; st.push_back(y * cw + x); }
    for (int y = 0; y < ch; y++) for (int x : {0, cw - 1}) if (key(x, y) && !seen[y * cw + x]) { seen[y * cw + x] = 1; st.push_back(y * cw + x); }
    while (!st.empty()) { const int i = st.back(); st.pop_back(); const int x = i % cw, y = i / cw;
      const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (auto& n : nb) { const int xx = x + n[0], yy = y + n[1];
        if (xx >= 0 && xx < cw && yy >= 0 && yy < ch && !seen[yy * cw + xx] && key(xx, yy)) { seen[yy * cw + xx] = 1; st.push_back(yy * cw + xx); } } }
    // Each walled-in pocket: a HOLE (a few pixels, or one pixel wide: a crack in him) is red;
    // a bigger pocket is air the drawing has (between the radio and his mouth), orange.
    int holes = 0, air = 0;
    std::vector<int> mark(cw * ch, 0);
    for (int i0 = 0; i0 < cw * ch; i0++) {
      if (seen[i0] || !key(i0 % cw, i0 / cw)) continue;
      std::vector<int> pk, s2{i0};
      seen[i0] = 1;
      int minx[200], maxx[200];
      for (int r = 0; r < ch; r++) { minx[r] = 999; maxx[r] = -1; }
      while (!s2.empty()) { const int i = s2.back(); s2.pop_back(); pk.push_back(i);
        const int x = i % cw, y = i / cw; if (x < minx[y]) minx[y] = x; if (x > maxx[y]) maxx[y] = x;
        const int nb[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto& n : nb) { const int xx = x + n[0], yy = y + n[1];
          if (xx >= 0 && xx < cw && yy >= 0 && yy < ch && !seen[yy * cw + xx] && key(xx, yy)) { seen[yy * cw + xx] = 1; s2.push_back(yy * cw + xx); } } }
      bool thin = true;
      for (int r = 0; r < ch; r++) if (maxx[r] > minx[r]) thin = false;
      const bool hole = pk.size() <= 5 || thin;
      for (int i : pk) mark[i] = hole ? 1 : 2;
      (hole ? holes : air) += (int)pk.size();
    }
    for (int y = 0; y < ch; y++) for (int x = 0; x < cw; x++) {
      uint16_t& px = d.px[(cy0 + y) * 320 + cx0 + x];
      if (px == KEYC) {
        if (mark[y * cw + x] == 1) px = 0xF800;                                   // a hole: red
        else if (mark[y * cw + x] == 2) px = 0xFC00;                              // drawn air: orange
        else px = ((x / 4 + y / 4) % 2) ? 0xFFE0 : 0x07FF;                       // bright checkerboard
      }
    }
    const int wfig = x1 - x0 + 1;
    int cut = 0;                                          // his pixels outside the 44 x 124 box
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++)
      if (d.px[y * 320 + x] != KEYC && (x < cx0 || x >= cx0 + cw || y < cy0)) cut++;
    fprintf(rep, "%s talk_%d%s: figure %dx%d px | in this box: %s (%d px outside) | holes: %d px | drawn air: %d px\n", pass ? "full width" : "44 x 124  ", k,
            mouth ? " (mouth open)" : "", wfig, y1 - y0 + 1, cut ? "NO" : "yes", cut, holes, air);
    char n[32]; snprintf(n, sizeof n, pass ? "qa_full_%d.ppm" : "qa_talk_%d.ppm", qn++ % 5);
    save(d, n, cx0, cy0, cw, ch);
  }
  fclose(rep);

  // ---- the sequence: walk -> talk -> walk ---------------------------------------------------------
  const int N = 110, LINE_AT = 22, TYPE_F = 12, LINE_F = 52;   // the line: typed over 12 frames, up for 52
  for (int f = 0; f < N; f++) {
    SquatchPose p;
    const char* say = nullptr;
    const int e = f - LINE_AT;
    if (e >= 0 && e < LINE_F) { p.talking = true; say = "Hey choom! Mesh is up."; if (e < TYPE_F) p.mouth = (e / 2) % 2; }
    d.fillRect(0, 0, 320, 240, t.bg);
    d.fillRect(0, 0, 320, 18, t.panel);
    d.setFont(&fonts::Font0); d.setTextColor(t.txt); d.drawString("RabbitMesh-WK", 6, 5); d.setTextColor(t.green); d.drawString("87%", 290, 5);
    scenes::mascotPose() = p;
    scenes::neonRabbit(d, t, f * 0.32f, f * 2.0f, false);
    scenes::mascotPose() = SquatchPose();
    // the bubble pops in a few frames after the line starts (as the lock screen's does)
    if (say && e >= 2) { int ax, ay; talk::anchor(t.style, 0, ax, ay); talk::bubble(d, t, ax, ay, say, 1.0f, e < TYPE_F ? (e * 2) : -1); }
    d.fillRect(0, scenes::GROUND + 2, 320, 240 - scenes::GROUND - 2, t.bg);
    d.setFont(&fonts::Font6); d.setTextColor(t.green); d.drawString("9:41", 105, scenes::GROUND + 9);
    const rabbit::Walker& w = rabbit::walker();
    static const char* ST[] = {"walking", "walking on to a passing step", "raising the radio", "talking", "lowering the radio"};
    char lab[64]; snprintf(lab, sizeof lab, "frame %3d   %s", f, ST[w.state]);
    d.setFont(&fonts::Font0); d.setTextColor(t.dim); d.drawString(lab, 6, 226);
    char n[32]; snprintf(n, sizeof n, "seq_%03d", f);
    save(d, std::string(n) + ".ppm"); saveText(std::string(n) + ".txt");
  }
  return 0;
}
