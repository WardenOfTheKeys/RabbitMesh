// MECP, the Mesh Emergency Communication Protocol: write and read its messages. Pure
// C++, host-tested in rabbitmesh/test/mecp_core_test.cpp.
//
// A MECP message is plain text, so it travels over any channel, and every reader with a
// code table sees it in their own language:
//
//   MECP/<severity>/<codes> [P05] [<n>pax] [<lat>,<lon>] [note]
//   severity 0 MAYDAY, 1 URGENT, 2 SAFETY, 3 ROUTINE
//   e.g. MECP/0/M01 S01 P05 3pax 36.8500,-76.2800 trail washed out
//
// The protocol is CC0 and the English code table below is from MECP
// (github.com/xiang-dev-1/MECP, languages/en.json) by xiang-dev-1, CC BY 4.0. The way
// the message is put together and read back follows WadaMesh's MECP app (GPL-3.0,
// github.com/ALLFATHER-BV/wadamesh, deploy/apps/mecp), which xiang-dev-1 also wrote.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

namespace mecpcore {

struct Code { const char* id; const char* text; };
struct Category { char id; const char* name; };

static const Category CATS[] = {
  {'M', "Medical"}, {'T', "Terrain / Infrastructure"}, {'W', "Weather / Environment"}, {'S', "Supplies"},
  {'P', "Position / Movement"}, {'C', "Coordination"}, {'R', "Response"}, {'D', "Drill / Test"},
  {'L', "Life / Leisure"}, {'X', "Threat / Security"}, {'H', "Have / Offer Resources"}, {'B', "Beacon"},
};
constexpr int NCATS = sizeof(CATS) / sizeof(CATS[0]);

static const Code CODES[] = {
  {"M01", "Injury"}, {"M02", "Unconscious person"}, {"M03", "Breathing difficulty"}, {"M04", "Cardiac event"},
  {"M05", "Hypothermia"}, {"M06", "Severe bleeding"}, {"M07", "Fracture / immobile"}, {"M08", "Burns"},
  {"M09", "Multiple casualties"}, {"M10", "Deceased"}, {"M11", "Animal bite / sting"},
  {"M12", "Allergic reaction / anaphylaxis"}, {"M13", "Poisoning / toxic exposure"}, {"M14", "Persons located alive"},
  {"M15", "Area searched, no victims found"},
  {"T01", "Road blocked"}, {"T02", "Bridge out"}, {"T03", "Building collapsed"}, {"T04", "Flooding"},
  {"T05", "Landslide"}, {"T06", "Power out"}, {"T07", "Fire"}, {"T08", "Avalanche"}, {"T09", "Path impassable"},
  {"T10", "Shelter available"}, {"T11", "Drowning / water rescue needed"}, {"T12", "Water contamination"},
  {"T13", "Earthquake"}, {"T14", "Gas leak"}, {"T15", "Chemical spill / HAZMAT"}, {"T16", "Vehicle accident"},
  {"T17", "Vehicle fire"},
  {"W01", "Storm approaching"}, {"W02", "Visibility zero"}, {"W03", "Extreme cold"}, {"W04", "Extreme heat"},
  {"W05", "Air quality danger"}, {"W06", "Tsunami / tidal surge warning"},
  {"S01", "Need water"}, {"S02", "Need food"}, {"S03", "Need medication"}, {"S04", "Need battery / power"},
  {"S05", "Need fuel"}, {"S06", "Need tools / equipment"},
  {"P01", "Stranded / stuck"}, {"P02", "Evacuating toward"}, {"P03", "Sheltering in place"}, {"P04", "En route to"},
  {"P05", "At GPS coordinates"}, {"P06", "Lost"}, {"P07", "Group separated"},
  {"C01", "Send rescue"}, {"C02", "Need transport"}, {"C03", "Relay this message"}, {"C04", "Confirm received"},
  {"C05", "How many people"}, {"C06", "What is status"}, {"C07", "Can you reach"}, {"C08", "Rendezvous at"},
  {"R01", "Acknowledged"}, {"R02", "Help coming"}, {"R03", "ETA [minutes]"}, {"R04", "Cannot assist"},
  {"R05", "Redirecting to"}, {"R06", "Stand by"}, {"R07", "Situation resolved / all clear"},
  {"D01", "This is a drill"}, {"D02", "This is a test"}, {"D03", "End of drill"}, {"D04", "Ignore previous, sent in error"},
  {"L01", "Beer / drinks"}, {"L02", "Coffee"}, {"L03", "Food ready"}, {"L04", "Summit reached"}, {"L05", "At camp"},
  {"L06", "Running late"}, {"L07", "Good signal here"}, {"L08", "Photo opportunity"}, {"L09", "Wildlife spotted"},
  {"L10", "Beautiful view"}, {"L11", "Trail conditions good"}, {"L12", "Trail conditions bad"}, {"L13", "Need a break"},
  {"L14", "Heading home"}, {"L15", "Good morning / check-in"}, {"L16", "Good night"}, {"L17", "Thank you"},
  {"L18", "Having fun"}, {"L19", "Festival / event here"}, {"L20", "Node test / ping"},
  {"X01", "Dangerous person / threat nearby"}, {"X02", "Area unsafe, avoid"}, {"X03", "Gunfire / explosions heard"},
  {"X04", "Civil unrest / crowd danger"}, {"X05", "Theft / looting reported"},
  {"X06", "Authorities / emergency services present"}, {"X07", "Checkpoint / road closure (authority)"},
  {"H01", "Have water available"}, {"H02", "Have food available"}, {"H03", "Have medical supplies"},
  {"H04", "Have power / charging"}, {"H05", "Have fuel"}, {"H06", "Have tools / equipment"},
  {"H07", "Have shelter / space for [N]pax"}, {"H08", "Have transport / vehicle"},
  {"B01", "Automated distress beacon active, sender may be unresponsive"},
  {"B02", "Beacon acknowledged, reduce transmission rate"}, {"B03", "Cancel beacon, I am OK"},
};
constexpr int NCODES = sizeof(CODES) / sizeof(CODES[0]);

static const char* const SEV[] = {"MAYDAY", "URGENT", "SAFETY", "ROUTINE"};
static const char* const SEV_SUB[] = {"life-threatening", "time-critical", "important", "normal"};
constexpr int NSEV = 4;

inline const char* codeText(const char* id) {
  for (const Code& c : CODES) if (!strncmp(c.id, id, 3) && strlen(id) == 3) return c.text;
  return nullptr;
}

// ---- writing -------------------------------------------------------------------------------
constexpr int MAX_CODES = 8;
struct Draft {
  uint8_t sev = 3;
  char codes[MAX_CODES][4] = {};
  uint8_t ncodes = 0;
  uint16_t pax = 0;
  bool withPos = true;
  char note[64] = "";

  int find(const char* id) const { for (int i = 0; i < ncodes; i++) if (!strcmp(codes[i], id)) return i; return -1; }
  // Adds the code, or takes it out if it was there. False if full.
  bool toggle(const char* id) {
    const int i = find(id);
    if (i >= 0) { for (int k = i; k + 1 < ncodes; k++) strcpy(codes[k], codes[k + 1]); ncodes--; return true; }
    if (ncodes >= MAX_CODES) return false;
    strncpy(codes[ncodes], id, 3); codes[ncodes][3] = 0; ncodes++;
    return true;
  }
};

// The message text for d, with a position if haveFix (and d.withPos). Returns its length.
inline int encode(const Draft& d, bool haveFix, double lat, double lon, char* out, size_t cap) {
  int n = snprintf(out, cap, "MECP/%u/", d.sev < NSEV ? d.sev : 3);
  auto add = [&](const char* s) {
    if (n < 0 || (size_t)n >= cap) return;
    const bool first = out[n - 1] == '/';
    n += snprintf(out + n, cap - n, first ? "%s" : " %s", s);
  };
  for (int i = 0; i < d.ncodes; i++) add(d.codes[i]);
  const bool pos = d.withPos && haveFix;
  if (pos && d.find("P05") < 0) add("P05");
  char b[40];
  if (d.pax) { snprintf(b, sizeof b, "%upax", (unsigned)d.pax); add(b); }
  if (pos) { snprintf(b, sizeof b, "%.4f,%.4f", lat, lon); add(b); }
  if (d.note[0]) add(d.note);
  return n < 0 ? 0 : ((size_t)n >= cap ? (int)cap - 1 : n);
}

// ---- reading -------------------------------------------------------------------------------
static bool isCodeTok(const char* t) { return strlen(t) == 3 && isupper((unsigned char)t[0]) && isdigit((unsigned char)t[1]) && isdigit((unsigned char)t[2]); }
static bool isPaxTok(const char* t) {
  const size_t n = strlen(t);
  if (n < 4 || strcmp(t + n - 3, "pax")) return false;
  for (size_t i = 0; i + 3 < n; i++) if (!isdigit((unsigned char)t[i])) return false;
  return true;
}
static bool isPosTok(const char* t, double* la, double* lo) {
  const char* c = strchr(t, ',');
  if (!c || c == t || !c[1]) return false;
  char* e1; char* e2;
  const double a = strtod(t, &e1), b = strtod(c + 1, &e2);
  if (e1 != c || *e2) return false;
  if (a < -90 || a > 90 || b < -180 || b > 180) return false;
  if (la) *la = a;
  if (lo) *lo = b;
  return true;
}

struct Decoded {
  int sev = -1;                 // -1: not a MECP message
  char words[240] = "";         // "MAYDAY: Injury, Need water, 3 people, at 36.85,-76.28 \"note\""
  bool hasPos = false;
  double lat = 0, lon = 0;
};

// Finds MECP/ anywhere in the text, so a "name: MECP/..." channel line reads too. Codes
// not in the table stay as they are.
inline bool decode(const char* text, Decoded& d) {
  d = Decoded();
  const char* m = text ? strstr(text, "MECP/") : nullptr;
  if (!m || !isdigit((unsigned char)m[5]) || m[6] != '/') return false;
  d.sev = m[5] - '0';
  char buf[200];
  strncpy(buf, m + 7, sizeof buf - 1); buf[sizeof buf - 1] = 0;
  size_t w = 0;
  auto put = [&](const char* s) {
    if (w >= sizeof d.words - 1) return;
    w += snprintf(d.words + w, sizeof d.words - w, "%s", s);
    if (w > sizeof d.words - 1) w = sizeof d.words - 1;
  };
  put(d.sev < NSEV ? SEV[d.sev] : "SEVERITY ");
  if (d.sev >= NSEV) { char s[4]; snprintf(s, sizeof s, "%d", d.sev); put(s); }
  put(":");
  char note[120] = "";
  bool atPos = false, any = false;
  for (char* tok = strtok(buf, " \t\r\n"); tok; tok = strtok(nullptr, " \t\r\n")) {
    char item[80] = "";
    double la, lo;
    if (!strcmp(tok, "P05")) { atPos = true; continue; }
    if (isCodeTok(tok)) { const char* t = codeText(tok); snprintf(item, sizeof item, "%s", t ? t : tok); }
    else if (isPaxTok(tok)) snprintf(item, sizeof item, "%d people", atoi(tok));
    else if (isPosTok(tok, &la, &lo)) { snprintf(item, sizeof item, "at %s", tok); d.hasPos = true; d.lat = la; d.lon = lo; atPos = false; }
    else if (tok[0] == '@' && strlen(tok) == 5 && isdigit((unsigned char)tok[1]) && isdigit((unsigned char)tok[4]))
      snprintf(item, sizeof item, "at %c%c:%c%c", tok[1], tok[2], tok[3], tok[4]);
    else {
      const size_t nl = strlen(note);
      snprintf(note + nl, sizeof note - nl, nl ? " %s" : "%s", tok);
      continue;
    }
    put(any ? ", " : " ");
    put(item);
    any = true;
  }
  if (atPos) { put(any ? ", " : " "); put(codeText("P05")); }
  if (note[0]) { put(" \""); put(note); put("\""); }
  return true;
}

}  // namespace mecpcore
