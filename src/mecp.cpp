// RabbitMesh MECP (see mecp.h).
#include "mecp.h"
#include <vector>
#include <algorithm>
#include <Preferences.h>
#include "app.h"
#include "node.h"
#include "history.h"
#include "gps.h"
#include "haptic.h"
#include "backlight.h"    // the board's Backlight and the idle dimmer (dimmer.idleFor, wake)
#include "audio_jingle.h"
#include "logstore.h"
#include "ui.h"
#include "deck_draw.h"

extern LogStore logs;

namespace mecp {

using namespace mecpcore;

static Draft s_d;
static int s_chan = -1;                     // channel index, -1: not picked yet
static bool s_loud = true;                  // MAYDAY/URGENT override a muted channel
static bool s_prefs = false;
static uint8_t s_urgent = 0;
static bool s_sirenUp = false;              // the siren is playing at full volume

static void loadPrefs() {
  if (s_prefs) return;
  s_prefs = true;
  Preferences p;
  if (p.begin("rm-mecp", true)) { s_chan = p.getInt("chan", -1); s_loud = p.getBool("loud", true); p.end(); }
}
static void savePrefs() {
  Preferences p;
  if (p.begin("rm-mecp", false)) { p.putInt("chan", s_chan); p.putBool("loud", s_loud); p.end(); }
}

uint8_t urgentWaiting() { return s_urgent; }

// ---- channel ---------------------------------------------------------------------------------
static int channel() {
  ChannelDetails ch;
  if (g_node && s_chan >= 0 && g_node->getChannel(s_chan, ch) && ch.name[0]) return s_chan;
  for (int i = 0; g_node && i < MAX_GROUP_CHANNELS; i++) if (g_node->getChannel(i, ch) && ch.name[0]) return i;
  return -1;
}
static String channelName(int idx) {
  ChannelDetails ch;
  if (idx < 0 || !g_node || !g_node->getChannel(idx, ch)) return String("none joined");
  char nm[40];
  sanitize(ch.name, nm, sizeof nm);
  return String(nm);
}

// ---- the message ---------------------------------------------------------------------------------
static bool fix(double& la, double& lo) {
  if (!gps.hasFix()) return false;
  la = gps.fix().lat; lo = gps.fix().lon;
  return !(la == 0 && lo == 0);
}
static int maxLen() {                       // MeshCore sends "<name>: <text>" in MAX_TEXT_LEN
  return g_node ? (int)MAX_TEXT_LEN - (int)strlen(g_node->name()) - 2 : 140;
}
static String text() {
  char out[200];
  double la = 0, lo = 0;
  const bool f = fix(la, lo);
  encode(s_d, f, la, lo, out, sizeof out);
  return String(out);
}

static void send() {
  if (!s_d.ncodes) { nav.toast("pick at least one code"); return; }
  const int idx = channel();
  if (idx < 0) { nav.toast("join a channel first", 3000); return; }
  const String t = text();
  if ((int)t.length() > maxLen()) { nav.toast((String("too long: ") + t.length() + " of " + maxLen()).c_str(), 3000); return; }
  ChannelDetails ch;
  g_node->getChannel(idx, ch);
  const uint32_t id = history.add(ConvKey::channel(ch.channel.secret), HF_OUT, ST_SENDING, g_node->name(), t.c_str(), app::now());
  if (!g_node->sendChannel(idx, t.c_str(), id)) {   // the radio's queue full, or the channel gone
    logs.add(LOG_WARN, "mecp NOT sent (%s): the radio couldn't queue it", SEV[s_d.sev % NSEV]);
    nav.toast((String(SEV[s_d.sev % NSEV]) + " NOT sent - try again").c_str(), 4000);
    return;
  }
  logs.add(s_d.sev <= 1 ? LOG_WARN : LOG_INFO, "mecp sent (%s)", SEV[s_d.sev % NSEV]);
  nav.toast((String(SEV[s_d.sev % NSEV]) + " sent to " + channelName(idx)).c_str(), 3000);
}

static void sendAsk() {
  if (s_d.sev == 0) {
    nav.push(new ConfirmView("Send a MAYDAY?", "life-threatening: everyone on the channel is alerted", [] { send(); }));
  } else send();
}

static void preview() {
  auto* v = new TextPageView("MECP preview", [](std::vector<String>& out) {
    const String t = text();
    out.push_back(t);
    out.push_back("");
    Decoded d;
    if (decode(t.c_str(), d)) out.push_back(d.words);
    out.push_back("");
    out.push_back(String(t.length()) + " of " + maxLen() + " characters" + ((int)t.length() > maxLen() ? "  - TOO LONG" : ""));
    out.push_back(String("to ") + channelName(channel()));
    double la, lo;
    if (s_d.withPos && !fix(la, lo)) out.push_back("no gps fix: it goes without a position");
    out.push_back("");
    out.push_back(s_d.sev == 0 ? "press: send MAYDAY (asks first)" : "press: send");
  }, 1000);
  v->onPress = [] { sendAsk(); };
  nav.push(v);
}

// ---- the code pickers ----------------------------------------------------------------------------
static void codesIn(char cat) {
  auto* m = new MenuView(String("MECP codes  ") + cat);
  static char s_cat;
  s_cat = cat;
  m->rebuild = [](MenuView& v) {
    for (const Code& c : CODES) {
      if (c.id[0] != s_cat) continue;
      const char* id = c.id;
      v.toggle(String(c.id) + "  " + c.text, [id] { return s_d.find(id) >= 0; }, [id] {
        if (!s_d.toggle(id)) nav.toast("eight codes at most");
      });
    }
  };
  m->rebuild(*m);
  nav.push(m);
}

static void categories() {
  auto* m = new MenuView("MECP categories");
  m->rebuild = [](MenuView& v) {
    v.info("picked", []() -> String {
      if (!s_d.ncodes) return String("none yet");
      String s;
      for (int i = 0; i < s_d.ncodes; i++) { if (i) s += " "; s += s_d.codes[i]; }
      return s;
    });
    for (const Category& c : CATS) {
      const char id = c.id;
      int n = 0;
      for (int i = 0; i < s_d.ncodes; i++) if (s_d.codes[i][0] == id) n++;
      v.submenu(String(id) + "  " + c.name, [id] { codesIn(id); }, [n]() -> String { return n ? String(n) : String(""); });
    }
  };
  m->rebuild(*m);
  nav.push(m);
}

// ---- reading: the inbox ----------------------------------------------------------------------------
static String convName(const ConvKey& k) {
  if (k.type != CONV_CHANNEL || !g_node) return String("direct");
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    ChannelDetails ch;
    if (g_node->getChannel(i, ch) && ch.name[0] && ConvKey::channel(ch.channel.secret) == k) {
      char nm[32];
      sanitize(ch.name, nm, sizeof nm);
      return String(nm[0] == '#' ? "" : "#") + nm;
    }
  }
  return String("a channel");
}

class InboxView : public View {
public:
  InboxView() { s_urgent = 0; collect(); }
  void tick() override { if (history.gen != _gen) collect(); }
  void draw(Canvas& g) override {
    char right[24];
    snprintf(right, sizeof right, "%d  press: %s", (int)_items.size(), _items.empty() ? "write" : "map");
    drawHeader(g, "MECP inbox", right);
    deckdraw::MecpModel m;
    m.items = _items.data(); m.n = (int)_items.size(); m.sel = _sel; m.first = _first;
    deckdraw::mecpInbox(g, nav.theme(), m);
  }
  void rotate(int d) override { select(_sel + d); }
  bool roll(int dx, int dy) override { select(_sel + dy + dx); return true; }
  void press() override {
    if (_items.empty()) { open(); return; }
    if (_sel < (int)_pos.size() && _items[_sel].hasPos) app::openMap(_pos[_sel].first, _pos[_sel].second, nullptr);
    else nav.toast("no position in this one");
  }
  void key(char c) override { if (c == 'w' || c == 'n') open(); }
  bool touch(const TouchEvent& e) override {
    if (e.type != TouchEvent::Tap || e.y < deckdraw::BODY_Y) return false;
    const int i = _first + (e.y - deckdraw::BODY_Y - 3) / 52;
    if (i >= 0 && i < (int)_items.size()) { if (i == _sel) press(); else select(i); }
    return true;
  }
private:
  std::vector<deckdraw::MecpItem> _items;
  std::vector<std::pair<double, double>> _pos;
  int _sel = 0, _first = 0;
  uint32_t _gen = 0;

  void select(int i) {
    if (_items.empty()) return;
    _sel = constrain(i, 0, (int)_items.size() - 1);
    const int rows = (deckdraw::H - deckdraw::BODY_Y - 4) / 52;
    if (_sel < _first) _first = _sel;
    if (_sel >= _first + rows) _first = _sel - rows + 1;
    dirty = true;
  }
  // Every MECP line in the history, newest first, at most 40.
  void collect() {
    _gen = history.gen;
    struct Found { uint32_t ts; deckdraw::MecpItem it; double la, lo; };
    std::vector<Found> found;
    static ConvKey convs[64];
    const uint16_t nc = history.conversations(convs, 64);
    static uint32_t ids[History::CAP];
    for (uint16_t c = 0; c < nc; c++) {
      const uint16_t n = history.collect(convs[c], ids, History::CAP);
      const String where = convName(convs[c]);
      for (uint16_t i = 0; i < n; i++) {
        const HistMsg* m = history.find(ids[i]);
        if (!m || !strstr(m->text, "MECP/")) continue;
        Decoded d;
        if (!decode(m->text, d)) continue;
        Found f = {};
        f.ts = m->ts;
        f.it.sev = (int8_t)d.sev;
        strlcpy(f.it.words, d.words, sizeof f.it.words);
        const bool mine = m->flags & HF_OUT;
        char from[24];
        sanitize(mine ? "you" : m->sender, from, sizeof from);
        snprintf(f.it.from, sizeof f.it.from, convs[c].type == CONV_CHANNEL ? "%s in %s" : "%s (%s)", from, where.c_str());
        strlcpy(f.it.age, timeAgo(m->ts), sizeof f.it.age);
        f.it.hasPos = d.hasPos; f.it.mine = mine;
        f.la = d.lat; f.lo = d.lon;
        found.push_back(f);
      }
    }
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.ts > b.ts; });
    if (found.size() > 40) found.resize(40);
    _items.clear(); _pos.clear();
    for (auto& f : found) { _items.push_back(f.it); _pos.push_back({f.la, f.lo}); }
    if (_sel >= (int)_items.size()) _sel = 0;
    dirty = true;
  }
};

void openInbox() { nav.push(new InboxView()); }

// ---- alerts ---------------------------------------------------------------------------------------
// A siren like the SOS countdown's, shorter: two sweeps.
static const ToneStep SIREN[] = {{880, 380, 1400}, {1400, 380, 880}, {880, 380, 1400}, {1400, 380, 880}};
static const Jingle SIREN_J = {"mecp", SIREN, 4, WAVE_SQUARE, false};

bool onChannelMessage(const char* channelName, const HistMsg& m) {
  if (m.flags & HF_OUT) return false;
  Decoded d;
  if (!strstr(m.text, "MECP/") || !decode(m.text, d)) return false;
  logs.add(d.sev <= 1 ? LOG_WARN : LOG_INFO, "mecp heard (%s)", d.sev < NSEV ? SEV[d.sev] : "?");
  if (d.sev > 1) return false;                     // SAFETY / ROUTINE: an ordinary message
  loadPrefs();
  if (s_urgent < 255) s_urgent++;
  char title[64], nm[32], from[24];
  sanitize(channelName, nm, sizeof nm);
  sanitize(m.sender, from, sizeof from);
  snprintf(title, sizeof title, "MECP %s  %s, %s", SEV[d.sev], from, nm);
  const char* w = strchr(d.words, ':');
  dimmer.wake();
  nav.banner(title, w ? w + 2 : d.words, 20000, [] { openInbox(); });
  haptic.buzz(3);
  if (s_loud) { jingle.setVolume(100); jingle.play(&SIREN_J); s_sirenUp = true; }   // even when muted
  return true;
}

void tick() {
  if (s_sirenUp && !jingle.playing()) { s_sirenUp = false; app::applySound(); }   // the user's volume again
}

// ---- the write menu --------------------------------------------------------------------------------
void open() {
  loadPrefs();
  auto* m = new MenuView("MECP");
  m->rebuild = [](MenuView& v) {
    v.action("inbox: decoded messages", [] { openInbox(); });
    v.header("write");
    v.adjust("severity", []() -> String { return String(SEV[s_d.sev % NSEV]) + "  " + SEV_SUB[s_d.sev % NSEV]; }, [](int d) {
      s_d.sev = (uint8_t)constrain((int)s_d.sev - d, 0, NSEV - 1);       // turn up: more serious
    });
    v.submenu("codes", [] { categories(); }, []() -> String {
      if (!s_d.ncodes) return String("pick");
      String s;
      for (int i = 0; i < s_d.ncodes; i++) { if (i) s += " "; s += s_d.codes[i]; }
      return s;
    });
    v.adjust("people", []() -> String { return s_d.pax ? String(s_d.pax) : String("-"); }, [](int d) {
      s_d.pax = (uint16_t)constrain((int)s_d.pax + d, 0, 999);
    });
    v.toggle("add my position", [] { return s_d.withPos; }, [] { s_d.withPos = !s_d.withPos; });
    v.value("note", []() -> String { return s_d.note[0] ? String(s_d.note) : String("none"); }, [] {
      prompt("MECP note", "plain words, short", s_d.note, sizeof(s_d.note) - 1, [](const String& t) {
        String n = t;
        n.replace("\n", " ");
        n.trim();
        strlcpy(s_d.note, n.c_str(), sizeof s_d.note);
      });
    });
    v.value("channel", []() -> String { return channelName(channel()); }, [] {
      const int idx = channel();
      for (int k = 1; k <= MAX_GROUP_CHANNELS; k++) {
        const int j = (idx + k) % MAX_GROUP_CHANNELS;
        ChannelDetails ch;
        if (g_node && g_node->getChannel(j, ch) && ch.name[0]) { s_chan = j; savePrefs(); break; }
      }
      nav.invalidate();
    });
    v.info("text", []() -> String { return text(); });
    v.action("preview and send", [] { preview(); });
    v.action("start over", [] { s_d = Draft(); nav.toast("cleared"); });
    v.header("read");
    v.action("decode a line", [] {
      prompt("Decode MECP", "paste or type MECP/...", "MECP/", 180, [](const String& t) {
        Decoded d;
        if (!decode(t.c_str(), d)) { nav.toast("not a MECP message"); return; }
        static char words[240];
        strlcpy(words, d.words, sizeof words);
        nav.push(new TextPageView("Decoded", [](std::vector<String>& out) { out.push_back(words); }, 5000));
      });
    });
    v.toggle("alerts: MAYDAY/URGENT sound even when muted", [] { return s_loud; }, [] { s_loud = !s_loud; savePrefs(); });
    v.info("protocol", []() -> String { return String("MECP by xiang-dev-1 (CC0, codes CC BY 4.0)"); });
  };
  m->rebuild(*m);
  nav.push(m);
}

}  // namespace mecp
