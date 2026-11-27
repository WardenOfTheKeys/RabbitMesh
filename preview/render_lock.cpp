// The RabbitMesh lock screen as LockView composes it (scenes, talk bubble, the T-Deck's
// clock layout), with the owner's rabbit sprite. Preview only; the time and texts are samples.
#include "themes.h"
#include "squatch_talk.h"
#include <cstdio>
#include <string>
Nav nav;
static void save(lgfx::LovyanGFX& d, const char* stem) {
  std::string p = std::string(stem) + ".ppm"; FILE* f = fopen(p.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
  p = std::string(stem) + ".txt"; f = fopen(p.c_str(), "w");
  for (auto& o : textOps()) fprintf(f, "%d\t%d\t%d\t%u\t%s\n", o.x, o.y, o.font, o.color, o.text.c_str());
  fclose(f); textOps().clear();
}
static void lock(LGFX& d, const char* stem, float phase, bool unread, const char* say, SquatchPose pose, const char* sub) {
  const Theme& t = nav.th;
  d.fillRect(0, 0, 320, 240, t.bg);
  d.fillRect(0, 0, 320, 18, t.panel);
  d.setFont(&fonts::Font0); d.setTextColor(t.txt); d.drawString("RabbitMesh-WK", 6, 5); d.setTextColor(t.green); d.drawString("87%", 290, 5);
  scenes::mascotPose() = pose;
  scenes::neonRabbit(d, t, phase, phase * 20, unread);
  scenes::mascotPose() = SquatchPose();
  if (say) { int ax, ay; talk::anchor(t.style, 0, ax, ay); talk::bubble(d, t, ax, ay, say, 1.0f); }
  d.fillRect(0, scenes::GROUND + 2, 320, 240 - scenes::GROUND - 2, t.bg);
  // the T-Deck's face under the scene, as LockView lays it out
  d.setFont(&fonts::Font6); const char* digits = "9:41"; const int wd = d.textWidth(digits);
  d.setFont(&fonts::Font4); const int wa = d.textWidth("pm") + 5;
  const int x = (320 - wd - wa) / 2, top = scenes::GROUND + 9;
  d.setFont(&fonts::Font6); d.setTextColor(t.green); d.drawString(digits, x, top);
  d.setFont(&fonts::Font4); d.setTextColor(scenes::mix(t.bg, t.green, 0.55f)); d.drawString("pm", x + wd + 5, top + 19);
  d.setFont(&fonts::Font2);
  const char* date = "Thu Nov 26";
  const int wDate = d.textWidth(date), wSub = sub ? d.textWidth(sub) : 0, gap = sub ? 16 : 0;
  const int lx = (320 - wDate - gap - wSub) / 2, ly = scenes::GROUND + 56;
  d.setTextColor(t.dim); d.drawString(date, lx, ly);
  if (sub) { d.setTextColor(t.amber); d.drawString(sub, lx + wDate + gap, ly); }
  d.setTextColor(t.greenDim); const char* q = "Follow the white rabbit."; d.drawString(q, (320 - d.textWidth(q)) / 2, ly + 16);
  save(d, stem);
}
// Frames of the walk at the lock screen's own pace (home.cpp: phase +0.32, scroll +2 a frame).
static void walkFrames(LGFX& d) {
  char stem[32];
  for (int f = 0; f < 40; f++) {
    SquatchPose p;
    const char* say = nullptr;
    if (f >= 14 && f < 32) { p.mouth = (f / 2) % 2; say = "Hey choom!"; }
    if (f >= 32) { p.wave = 1; say = "Ears up: a message!"; }
    snprintf(stem, sizeof stem, "walk_%02d", f);
    lock(d, stem, f * 0.32f, f >= 32, say, p, f >= 32 ? "1 unread message" : nullptr);
  }
  // a close-up strip: eight steps of one stride, side by side
  const Theme& t = nav.th;
  d.fillRect(0, 0, 320, 240, t.bg);
  for (int i = 0; i < 6; i++) {
    d.drawFastHLine(i * 53 + 2, 200, 50, t.green);
    drawRabbit(d, t, i * 53 + 27, 200, rabbitsprite::H, i * 1.047f, t.green, SquatchPose());
  }
  save(d, "stride");
}

int main(int argc, char** argv) {
  LGFX d;
  const ThemeSpec& spec = THEMES[THEME_COUNT - 1];
  nav.th.apply(d, spec.palette, spec.style); nav.th.scene = spec.scene;
  SquatchPose talkp; talkp.mouth = 1;
  SquatchPose wave; wave.wave = 1;
  if (argc > 1) { walkFrames(d); return 0; }
  lock(d, "lock_idle", 0.2f, false, nullptr, SquatchPose(), nullptr);
  lock(d, "lock_talk", 1.3f, false, "Hey choom!", talkp, nullptr);
  lock(d, "lock_unread", 2.1f, true, "Incoming transmission!", wave, "2 unread messages");
  // poses side by side, at the size he's drawn
  const Theme& t = nav.th;
  d.fillRect(0, 0, 320, 240, t.bg);
  SquatchPose P[6]; P[1].blink = true; P[2].happy = true; P[3].slump = 1; P[4].dizzy = 0.6f; P[5].wave = 1;
  for (int i = 0; i < 6; i++) { d.drawFastHLine(i * 53 + 4, 180, 46, t.line); drawRabbit(d, t, i * 53 + 27, 180, rabbitsprite::H, 1.0f + i, i == 0 ? t.green : t.green, P[i]); }
  d.setFont(&fonts::Font0); d.setTextColor(t.dim);
  const char* names[] = {"idle", "blink", "happy", "tired", "dizzy", "on air"};
  for (int i = 0; i < 6; i++) d.drawString(names[i], i * 53 + 10, 190);
  save(d, "poses");
}
