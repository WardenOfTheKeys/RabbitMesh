// RabbitMesh "Burrow Breaker" on the T-Deck (see bricks.h).
#include "bricks.h"
#include <Preferences.h>
#include <esp_heap_caps.h>
#include "app.h"
#include "backlight.h"    // the idle dimmer: the game holds while the screen is dark
#include "audio_jingle.h"
#include "ui.h"
#include "bricks_core.h"
#include "bricks_draw.h"

namespace bricks {

// Short blips: the player starts the codec for each, so they're kept few and short, and one
// asked for while another plays is simply skipped.
static ToneStep s_blip[] = {{900, 14}};
static const Jingle BLIP_J = {"bb-blip", s_blip, 1, WAVE_SQUARE, false};
static const ToneStep s_pick[] = {{880, 40}, {1320, 60}};
static const Jingle PICK_J = {"bb-pick", s_pick, 2, WAVE_TRIANGLE, false};
static const ToneStep s_lost[] = {{330, 90, 110}};
static const Jingle LOST_J = {"bb-lost", s_lost, 1, WAVE_SQUARE, false};
static const ToneStep s_clear[] = {{660, 70}, {880, 70}, {1320, 140}};
static const Jingle CLEAR_J = {"bb-clear", s_clear, 3, WAVE_TRIANGLE, false};
static const ToneStep s_over[] = {{440, 120}, {330, 120}, {220, 240}};
static const Jingle OVER_J = {"bb-over", s_over, 3, WAVE_TRIANGLE, false};

static constexpr uint32_t STEP_MS = 8;            // the game moves in steps of 8 ms
static constexpr float ROLL_PX = 18.0f;           // one trackball click moves the board this far

class BricksView : public View {
public:
  BricksView() {
    Preferences p;
    if (p.begin("rm-game", true)) { _best = p.getULong("best", 0); _sound = p.getBool("sound", true); p.end(); }
    _saved = _best; _savedSound = _sound;
    // the background, unpacked once (150 KB, in PSRAM): each frame is then one copy of it
    _bg = (uint16_t*)heap_caps_malloc(bricksart::W * bricksart::H * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (_bg) unpackBackground(_bg);
    _g.seed(esp_random());
    _g.newGame();
    _last = millis();
  }
  ~BricksView() override { save(); if (_bg) heap_caps_free(_bg); }

  void tick() override {
    const uint32_t now = millis();
    uint32_t el = now - _last;
    _last = now;
    if (dimmer.asleep()) { if (_g.state == PLAY) _g.pause(); return; }   // the screen went dark: hold it
    if (el > 100) el = 100;                                              // a slow loop pass: don't jump
    _acc += el;
    uint16_t ev = 0;
    while (_acc >= STEP_MS) {
      _acc -= STEP_MS;
      _g.step(STEP_MS / 1000.0f);
      ev |= _g.events;
      _sparks.add(_g);
      _sparks.step(STEP_MS / 1000.0f);
    }
    if (ev) sounds(ev);
    if (ev & E_OVER) save();
    if (now - _drawn >= 30) { _drawn = now; dirty = true; }
  }
  void resume() override { if (_g.state == PLAY) _g.pause(); _last = millis(); dirty = true; }

  // The whole screen, as the art has it: its own title bar where the status bar and header
  // would be. Toasts and message banners still show over it.
  void draw(Canvas& g) override {
    if (!_bg) g.fillScreen(bricksart::C_BG);
    bricks::draw(g, _g, _sparks, _bg, _best, _sound);
  }

  void rotate(int d) override { _g.nudge(d * ROLL_PX); }
  bool roll(int dx, int dy) override { (void)dy; if (dx) _g.nudge(dx * ROLL_PX); return true; }
  void press() override { _g.press(); dirty = true; }
  void key(char c) override {
    switch (tolower((unsigned char)c)) {
      case 'a': case 'j': case ',': _g.nudge(-ROLL_PX * 1.5f); break;
      case 'd': case 'l': case '.': _g.nudge(ROLL_PX * 1.5f); break;
      case ' ': case '\n': case '\r': _g.press(); break;
      case 'p': _g.pause(); break;
      case 's':
        _sound = !_sound;
        nav.toast(_sound ? "sound on" : "sound off", 1200);
        save();
        break;
    }
    dirty = true;
  }
  // A finger on the screen: the board follows it; a tap launches.
  bool touch(const TouchEvent& e) override {
    if (e.type == TouchEvent::Down || e.type == TouchEvent::Drag) { _g.setTarget(e.x); return true; }
    if (e.type == TouchEvent::Tap) { _g.setTarget(e.x); _g.press(); return true; }
    return false;
  }
  bool wantsAllKeys() override { return true; }   // a, d, p, s are the game's
  bool coasts() override { return false; }

private:
  Game _g;
  Sparks _sparks;
  uint16_t* _bg = nullptr;
  uint32_t _best = 0, _last = 0, _acc = 0, _drawn = 0, _saved = 0;
  bool _sound = true;

  void sounds(uint16_t ev) {
    if (!_sound) return;
    if (ev & E_OVER) jingle.play(&OVER_J);
    else if (ev & E_CLEAR) jingle.play(&CLEAR_J);
    else if (ev & E_LOST) jingle.play(&LOST_J);
    else if (ev & E_PICKUP) jingle.play(&PICK_J);
    else if (ev & (E_BRICK | E_ICE)) { s_blip[0].freq = (ev & E_ICE) ? 1400 : 900; jingle.play(&BLIP_J); }
    else if (ev & E_PADDLE) { s_blip[0].freq = 520; jingle.play(&BLIP_J); }
  }
  // The best score and the sound setting, kept across restarts (written only when they change).
  void save() {
    if (_g.score > _best) _best = _g.score;
    if (_best == _saved && !_soundDirty()) return;
    Preferences p;
    if (!p.begin("rm-game", false)) return;
    p.putULong("best", _best);
    p.putBool("sound", _sound);
    p.end();
    _saved = _best;
    _savedSound = _sound;
  }
  bool _savedSound = true;
  bool _soundDirty() const { return _sound != _savedSound; }
};

void open() { nav.push(new BricksView()); }

}  // namespace bricks
