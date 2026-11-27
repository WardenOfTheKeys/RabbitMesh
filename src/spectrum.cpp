// RabbitMesh spectrum analyzer (see spectrum.h). The sweep follows what WadaMesh found
// on hardware: retune, arm receive, wait ~1.5 ms for the instantaneous RSSI to settle,
// then keep the peak of several reads; a read of 0 dBm is a failed SPI read, not a
// signal. Handing back: recalibrate the image filter for the mesh frequency, re-apply
// the mesh settings and re-arm receive - MeshCore's wrapper may still believe it is
// receiving, and without the re-arm the node would go deaf.
#include "spectrum.h"
#include <esp_heap_caps.h>
#include <target.h>
#include "app.h"
#include "node.h"
#include "logstore.h"
#include "fieldtools.h"
#include "wardrive.h"
#include "backlight.h"    // the board's Backlight and the idle dimmer (dimmer.idleFor, wake)
#include "ui.h"
#include "deck_draw.h"

extern RADIO_CLASS radio;      // variants/inw_tdeck/target.cpp
extern LogStore logs;

namespace spectrum {

using namespace speccore;

static bool s_owns = false;
static speccore::Sweep* s_sw = nullptr;
static Span s_span;
static int s_spanIdx = 2;                    // SPANS[2]: 24 MHz
static uint32_t s_viewAt = 0;                // millis the view last ticked
static constexpr uint32_t TICK_BUDGET_US = 40000;   // sweep at most this long per loop pass
static constexpr int SETTLE_US = 1500, READS = 6, READ_GAP_US = 400;

bool ownsRadio() { return s_owns; }

static float meshMHz() { return g_node ? g_node->prefs().freq : 910.525f; }

static bool acquire() {
  if (s_owns) return true;
  if (!g_node) { nav.toast("radio not running"); return false; }
  if (wardrive::running() || field::rangeOn()) {
    nav.toast("stop the wardrive / range test first - they need the radio", 3000);
    return false;
  }
  s_span = makeSpan(meshMHz(), SPANS[s_spanIdx]);
  radio.standby();                           // config commands are standby commands
  radio.setBandwidth(RBW_KHZ);               // the narrowest filter: neighbouring bins resolve
  radio.setRxBoostedGainMode(true);
  s_owns = true;
  logs.add(LOG_INFO, "spectrum: radio borrowed, mesh paused");
  return true;
}

static void release() {
  if (!s_owns) return;
  radio.standby();
  radio.calibrateImage(meshMHz());           // the sweep left it calibrated somewhere else
  g_node->applyRadio();                      // frequency, bandwidth, SF, CR, power, gain
  radio.startReceive();                      // MeshCore's own (with preamble detect): receive again
  s_owns = false;                            // only now may the mesh touch the radio
  logs.add(LOG_INFO, "spectrum: radio handed back to the mesh");
}

// One pass of the sweep, up to TICK_BUDGET_US.
static void sweepSome() {
  const uint32_t t0 = micros();
  do {
    const int i = s_sw->pos;
    if (radio.setFrequency(s_span.freq(i)) == RADIOLIB_ERR_NONE) {
      // Receive with no interrupt routed to DIO1: a LoRa packet caught mid-sweep must not
      // leave MeshCore a "packet ready" flag to read after we hand back.
      radio.SX1262::startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF, RADIOLIB_IRQ_RX_DEFAULT_FLAGS, 0, 0);
      delayMicroseconds(SETTLE_US);
      int peak = -200;
      for (int k = 0; k < READS; k++) {
        const int r = (int)radio.getRSSI(false);   // instantaneous channel RSSI, dBm
        if (r <= DBM_HI && r >= DBM_LO && r > peak) peak = r;
        delayMicroseconds(READ_GAP_US);
      }
      if (peak > -200) s_sw->put(i, peak);
    }
    s_sw->next();                            // a failed retune keeps the bin's old value
  } while (micros() - t0 < TICK_BUDGET_US);
}

void watch() {
  // The screen isn't on top any more (a banner, a call to another screen) or the display
  // went dark: the mesh gets its radio back. The view takes it again when it ticks.
  if (s_owns && (millis() - s_viewAt > 1500 || dimmer.asleep())) release();
}

class SpectrumView : public View {
public:
  SpectrumView() {
    if (!s_sw) {
      s_sw = (Sweep*)heap_caps_malloc(sizeof(Sweep), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
      if (!s_sw) s_sw = (Sweep*)malloc(sizeof(Sweep));
    }
    if (s_sw) s_sw->reset();
    _cursor = BINS / 2;
    s_viewAt = millis();
    _ok = s_sw && acquire();
  }
  ~SpectrumView() override { release(); }
  void tick() override {
    s_viewAt = millis();
    if (!_ok || !s_sw) return;
    if (!_hold && !dimmer.asleep()) {
      if (!s_owns && !acquire()) { _ok = false; return; }
      sweepSome();
    }
    if (millis() - _drawn > 120) { _drawn = millis(); dirty = true; }
  }
  void draw(Canvas& g) override {
    const Theme& t = nav.theme();
    drawHeader(g, "Spectrum", !_ok ? "radio busy" : _hold ? "hold - mesh on" : "MESH PAUSED");
    if (!s_sw) return;
    deckdraw::SpecModel m;
    m.s = s_sw; m.span = s_span; m.spanMHz = SPANS[s_spanIdx]; m.meshMHz = meshMHz();
    m.cursor = _cursor; m.frozen = _hold; m.owns = s_owns; m.phase = millis() / 1000.0f;
    deckdraw::spectrum(g, t, m);
  }
  // Left/right: the cursor. Up/down: the span. Press: hold the picture and give the mesh
  // its radio back; press again to sweep on. 'p': clear the peak hold. 'c': centre on
  // the cursor's frequency.
  void rotate(int d) override { moveCursor(d); }
  bool roll(int dx, int dy) override {
    if (dx) moveCursor(dx * 2);
    if (dy) setSpan(s_spanIdx - (dy > 0 ? 1 : -1));
    return true;
  }
  void press() override {
    _hold = !_hold;
    if (_hold) release(); else if (!acquire()) _hold = true;
    dirty = true;
  }
  void key(char c) override {
    if (c == 'p' && s_sw) { for (int i = 0; i < BINS; i++) s_sw->peak[i] = s_sw->rssi[i]; }
    if (c == 'c') recentre(s_span.freq(_cursor));
    if (c == 'm') recentre(meshMHz());
    if (c == '+' || c == 'z') setSpan(s_spanIdx - 1);
    if (c == '-' || c == 'x') setSpan(s_spanIdx + 1);
    dirty = true;
  }
  bool touch(const TouchEvent& e) override {
    if (e.type == TouchEvent::Tap && e.y > deckdraw::BODY_Y) {
      const int px = 26, pw = deckdraw::W - px - 4;
      if (e.x >= px) { _cursor = constrain((e.x - px) * BINS / pw, 0, BINS - 1); dirty = true; }
      return true;
    }
    if (e.type == TouchEvent::LongPress) { press(); return true; }
    return false;
  }
  bool coasts() override { return false; }
private:
  int _cursor = BINS / 2;
  bool _hold = false, _ok = false;
  uint32_t _drawn = 0;

  void moveCursor(int d) { _cursor = constrain(_cursor + d, 0, BINS - 1); dirty = true; }
  void setSpan(int idx) {
    idx = constrain(idx, 0, NSPANS - 1);
    if (idx == s_spanIdx) return;
    const float c = s_span.freq(_cursor);
    s_spanIdx = idx;
    recentre(c);
  }
  void recentre(float mhz) {
    s_span = makeSpan(mhz, SPANS[s_spanIdx]);
    if (s_sw) s_sw->reset();
    _cursor = s_span.bin(mhz);
    dirty = true;
  }
};

void open() { nav.push(new SpectrumView()); }

}  // namespace spectrum
