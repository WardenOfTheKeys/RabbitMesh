// RabbitMesh SD card viewer (see sdview.h).
#include "sdview.h"
#include <SD.h>
#include <vector>
#include <algorithm>
#include "dataio.h"
#include "logstore.h"
#include "ui.h"

extern LogStore logs;

namespace sdview {

// What each tool writes, and where.
struct Folder { const char* path; const char* what; };
static const Folder FOLDERS[] = {
  {"/rabbit/wigle", "wifi + bt wardrive (wigle)"},
  {"/rabbit/wardrive", "mesh coverage surveys"},
  {"/rabbit/wardrive/export", "survey exports (kml, geojson)"},
  {"/rabbit/tracks", "trip tracks (gpx)"},
  {"/rabbit/packets", "packet logs"},
  {"/rabbit/flock", "flock hunt detections"},
  {"/rabbit/alpr", "camera map"},
  {"/inw/range", "range tests"},
  {"/inw/trail", "breadcrumb trails"},
  {"/screenshots", "screenshots"},
};

static String size(uint64_t b) {
  char s[24];
  if (b >= 1000ULL * 1000 * 1000) snprintf(s, sizeof s, "%.1f GB", b / 1e9);
  else if (b >= 1000ULL * 1000) snprintf(s, sizeof s, "%.1f MB", b / 1e6);
  else if (b >= 1000) snprintf(s, sizeof s, "%.0f KB", b / 1e3);
  else snprintf(s, sizeof s, "%u B", (unsigned)b);
  return String(s);
}
static String baseName(const String& p) { return p.substring(p.lastIndexOf('/') + 1); }

struct Entry { String path; uint32_t bytes; };
// The files directly in a folder, newest first (the tools name them by date and time).
static std::vector<Entry> list(const char* dir, uint64_t* total = nullptr) {
  std::vector<Entry> out;
  if (total) *total = 0;
  if (!sdMounted()) return out;
  File d = SD.open(dir);
  if (!d || !d.isDirectory()) return out;
  for (File f = d.openNextFile(); f; f = d.openNextFile()) {
    if (!f.isDirectory()) {
      out.push_back({String(dir) + "/" + baseName(f.name()), (uint32_t)f.size()});
      if (total) *total += f.size();
    }
    f.close();
  }
  d.close();
  std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) { return baseName(a.path) > baseName(b.path); });
  return out;
}

// For the Deck's tile: worked out when the Deck opens, not each frame (see open()).
String tileText() {
  if (!sdMounted()) return String("no card");
  return size(SD.totalBytes() - SD.usedBytes()) + " free";
}

// The start of a text file, for a look without a computer.
static void viewFile(const String& path) {
  static std::vector<String> lines;
  static String name;
  lines.clear();
  name = baseName(path);
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) { nav.toast("can't open that file"); return; }
  char buf[124];
  int bin = 0;
  while (f.available() && lines.size() < 80) {
    const size_t n = f.readBytesUntil('\n', buf, sizeof buf - 1);
    buf[n] = 0;
    for (size_t i = 0; i < n; i++) if ((uint8_t)buf[i] < 9 || ((uint8_t)buf[i] > 13 && (uint8_t)buf[i] < 32)) { buf[i] = '.'; bin++; }
    lines.push_back(String(buf));
  }
  const bool more = f.available();
  f.close();
  if (bin > 40) { lines.clear(); lines.push_back("a binary file (the camera map, a picture...)"); lines.push_back("open it on a computer"); }
  else if (more) lines.push_back("#... the rest: open it on a computer");
  nav.push(new TextPageView(name, [](std::vector<String>& out) { out = lines; }, 60000));
}

static void fileMenu(const String& path, uint32_t bytes) {
  auto* m = new MenuView(baseName(path));
  static String s_path;
  static uint32_t s_bytes;
  s_path = path; s_bytes = bytes;
  m->rebuild = [](MenuView& v) {
    v.info("size", []() -> String { return size(s_bytes); });
    v.info("folder", []() -> String { return s_path.substring(0, s_path.lastIndexOf('/')); });
    v.action("look at the start of it", [] { viewFile(s_path); });
    v.action("delete it", [] {
      confirm("Delete this file?", baseName(s_path), [] {
        if (SD.remove(s_path.c_str())) { logs.add(LOG_INFO, "sd: deleted %s", s_path.c_str()); nav.toast("deleted"); nav.pop(); }
        else nav.toast("couldn't delete it");
      });
    });
  };
  m->rebuild(*m);
  nav.push(m);
}

static void folderMenu(const Folder& fo) {
  static const Folder* s_fo;
  s_fo = &fo;
  auto* m = new MenuView(fo.what);
  m->rebuild = [](MenuView& v) {
    uint64_t total = 0;
    const std::vector<Entry> files = list(s_fo->path, &total);
    const String inIt = String(files.size()) + (files.size() == 1 ? " file, " : " files, ") + size(total);
    v.info("in it", [inIt]() -> String { return inIt; });
    if (files.empty()) return;
    for (size_t i = 0; i < files.size() && i < 60; i++) {
      const Entry e = files[i];
      const String sz = size(e.bytes);
      v.submenu(baseName(e.path), [e] { fileMenu(e.path, e.bytes); }, [sz]() -> String { return sz; });
    }
    if (files.size() > 60) {
      const String older = String((unsigned)(files.size() - 60)) + " older (on a computer)";
      v.info("and", [older]() -> String { return older; });
    }
    v.action("delete everything in here", [] {
      confirm("Delete all of these?", String(s_fo->what) + ": every file in " + s_fo->path, [] {
        int n = 0;
        for (const Entry& e : list(s_fo->path)) if (SD.remove(e.path.c_str())) n++;
        logs.add(LOG_INFO, "sd: deleted %d files in %s", n, s_fo->path);
        nav.toast((String("deleted ") + n + " files").c_str());
      });
    });
  };
  m->rebuild(*m);
  nav.push(m);
}

// A card that's in but can't be read: say why, and offer to make it one the T-Deck reads.
static void unreadable() {
  auto* m = new MenuView("SD card");
  m->info("card", []() -> String { return String("in, but can't be read"); });
  m->info("why", []() -> String { return String("not FAT32 (exFAT or a Mac format)"); });
  m->info("fix", []() -> String { return String("format it here, or as FAT32 on a computer"); });
  m->action("format it as FAT32 (erases it)", [] {
    confirm("Format the SD card?", "everything on the card is erased. it becomes FAT32, which the T-Deck reads. large cards take a minute",
            [] {
              nav.busy("formatting the card...");
              const char* r = sdFormat();
              logs.add(LOG_INFO, "sd: format from the viewer: %s", r);
              nav.toast(!strcmp(r, "formatted") ? "formatted: back, then open SD card again" : r, 4000);
            });
  });
  nav.push(m);
}

void open() {
  sdLookNow();   // a card put in a moment ago is looked at now, not in 10 s
  if (!sdMount()) {
    if (sdState() == SD_UNREADABLE) unreadable();
    else nav.toast("no sd card found: push it in until it clicks, then try again", 3500);
    return;
  }
  auto* m = new MenuView("SD card");
  m->rebuild = [](MenuView& v) {
    if (!sdMounted()) { v.info("card", []() -> String { return String("not found"); }); return; }
    // Worked out once here: on a big card counting the used space reads the whole FAT (seconds).
    const uint64_t used = SD.usedBytes(), all = SD.totalBytes();
    const String u = size(used) + " of " + size(all), fr = size(all > used ? all - used : 0);
    v.info("used", [u]() -> String { return u; });
    v.info("free", [fr]() -> String { return fr; });
    v.header("what the tools saved");
    for (const Folder& fo : FOLDERS) {
      uint64_t total = 0;
      const size_t n = list(fo.path, &total).size();
      if (!n) continue;
      const String val = String((unsigned)n) + "  " + size(total);
      const Folder* p = &fo;
      v.submenu(fo.what, [p] { folderMenu(*p); }, [val]() -> String { return val; });
    }
  };
  m->rebuild(*m);
  nav.push(m);
}

}  // namespace sdview
