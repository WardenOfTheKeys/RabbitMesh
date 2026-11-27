// Host test for mecp_core.h
#include "mecp_core.h"
#include <cstdio>
#include <cmath>
using namespace mecpcore;
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main() {
  CHECK(NCODES == 108 && NCATS == 12);   // the whole English table, as WadaMesh ships it
  // every code's category exists, and ids are unique
  for (int i = 0; i < NCODES; i++) {
    bool cat = false; for (auto& c : CATS) if (c.id == CODES[i].id[0]) cat = true;
    CHECK(cat);
    for (int j = i + 1; j < NCODES; j++) CHECK(strcmp(CODES[i].id, CODES[j].id));
  }
  CHECK(!strcmp(codeText("S01"), "Need water") && codeText("Z99") == nullptr && codeText("S0") == nullptr);

  Draft d; d.sev = 0;
  CHECK(d.toggle("M01") && d.toggle("S01") && d.ncodes == 2);
  CHECK(d.toggle("M01") && d.ncodes == 1 && !strcmp(d.codes[0], "S01"));   // toggled off
  d.toggle("M01");
  d.pax = 3; strcpy(d.note, "trail washed out");
  char out[200];
  int n = encode(d, true, 36.85, -76.28, out, sizeof out);
  CHECK(!strcmp(out, "MECP/0/S01 M01 P05 3pax 36.8500,-76.2800 trail washed out") && n == (int)strlen(out));
  encode(d, false, 0, 0, out, sizeof out);                   // no fix: no position, no P05
  CHECK(!strcmp(out, "MECP/0/S01 M01 3pax trail washed out"));
  Draft r; r.toggle("R01");
  encode(r, true, 1, 2, out, sizeof out); r.withPos = false;
  encode(r, true, 1, 2, out, sizeof out);
  CHECK(!strcmp(out, "MECP/3/R01"));
  char tiny[12]; encode(d, true, 36.85, -76.28, tiny, sizeof tiny);   // truncates safely
  CHECK(strlen(tiny) == 11);
  Draft full; char id[4];
  for (int i = 1; i <= MAX_CODES; i++) { snprintf(id, sizeof id, "L%02d", i); CHECK(full.toggle(id)); }
  CHECK(!full.toggle("L20"));

  Decoded x;
  CHECK(decode("Bob: MECP/0/M01 S01 P05 3pax 36.8500,-76.2800 trail washed out", x));
  CHECK(x.sev == 0 && x.hasPos && fabs(x.lat - 36.85) < 1e-9 && fabs(x.lon + 76.28) < 1e-9);
  CHECK(!strcmp(x.words, "MAYDAY: Injury, Need water, 3 people, at 36.8500,-76.2800 \"trail washed out\""));
  CHECK(decode("MECP/1/P05 Z42 @1430", x));
  CHECK(!strcmp(x.words, "URGENT: Z42, at 14:30, At GPS coordinates"));
  CHECK(decode("MECP/7/D02", x) && !strcmp(x.words, "SEVERITY 7: This is a test"));
  CHECK(!decode("hello world", x) && x.sev == -1);
  CHECK(!decode("MECP/x/M01", x) && !decode("MECP/", x) && !decode(nullptr, x));
  // a message we write reads back
  encode(d, true, 36.85, -76.28, out, sizeof out);
  CHECK(decode(out, x) && x.sev == 0 && x.hasPos);
  printf(fails ? "%d FAILED\n" : "all mecp_core tests passed\n", fails);
  return fails != 0;
}
