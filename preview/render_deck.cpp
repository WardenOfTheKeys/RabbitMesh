// Renders the RabbitMesh deck screens with SAMPLE data, through the real deck_draw.h.
// Preview only: the numbers are made up to fill the screens.
#include "themes.h"
#include "deck_draw.h"
#include <cstdio>
#include <string>
using namespace deckdraw;

static void save(lgfx::LovyanGFX& d, const char* stem, const Theme& t, const char* title) {
  // the status bar and header the real UI draws around a view
  d.fillRect(0, 0, W, 18, t.panel);
  d.setFont(&fonts::Font2); d.setTextColor(t.green);
  d.drawString("<", 6, 22); d.setTextColor(t.txt); d.drawString(title, 18, 22);
  d.drawFastHLine(0, BODY_Y - 1, W, t.line);
  d.setFont(&fonts::Font0); d.setTextColor(t.dim); d.drawString("9:41", W - 30, 5); d.drawString("RabbitMesh", 6, 5);
  std::string p = std::string(stem) + ".ppm";
  FILE* f = fopen(p.c_str(), "wb");
  fprintf(f, "P6 %d %d 255\n", d.W, d.H);
  for (uint16_t c : d.px) { unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
  fclose(f);
  p = std::string(stem) + ".txt";
  f = fopen(p.c_str(), "w");
  for (auto& o : textOps()) fprintf(f, "%d\t%d\t%d\t%u\t%s\n", o.x, o.y, o.font, o.color, o.text.c_str());
  fclose(f);
  textOps().clear();
}

int main() {
  LGFX d;
  Theme t;
  const ThemeSpec& spec = THEMES[THEME_COUNT - 1];
  t.apply(d, spec.palette, spec.style); t.scene = spec.scene;
  auto clear = [&] { d.fillRect(0, 0, W, H, t.bg); };

  // hub
  Tile tiles[14] = {
    {"DRIVE", "12.4 mi  0h42m", IC_DRIVE, true, 'd'},   {"WARDRV", "83 stops", IC_WARDRIVE, true, 'w'},
    {"WIFI/BT", "412 wifi 96bt", IC_WIFI, true, 'b'},  {"FLOCK", "1830 mapped", IC_FLOCK, false, 'f'},
    {"FOX", "find a signal", IC_FOX, false, 'x'},       {"TRIP", "12.4 mi  REC", IC_TRIP, true, 't'},
    {"RF MON", "noise -121", IC_RF, false, 'r'},    {"SPECTRUM", "sweep band", IC_SPECTRUM, false, 's'},
    {"AIRTIME", "load 3.4%", IC_AIR, false, 'a'},       {"RADAR", "12 placed", IC_RADAR, false, 'n'},
    {"MECP", "1 urgent", IC_MECP, true, 'e'},           {"FIELD", "sos range trl", IC_FIELD, false, 'i'},
    {"SD CARD", "27.3 GB free", IC_SD, false, 'c'},     {"MORE", "all tools", IC_MORE, false, 'o'}};
  clear(); hub(d, t, tiles, 14, 0, 1.3f); save(d, "deck_hub", t, "Deck");

  // monitor
  deckcore::Series<96> rs, ns;
  for (int i = 0; i < 40; i++) rs.push((int16_t)(-104 + (int)(14 * sinf(i * 0.7f)) + (i * 37 % 11) - 5));
  for (int i = 0; i < 96; i++) ns.push((int16_t)(-121 + (i * 13 % 5) - 2));
  MonitorModel mm; mm.rssi = -97; mm.noise = -121; mm.snr = 6.5f; mm.marginDb = 14.0f; mm.perMin = 23; mm.heard = true;
  mm.rssiS = &rs; mm.noiseS = &ns;
  const char* rows[] = {"2s ADVERT  flood  3hop -97dBm 6.5dB", "9s GRP_TXT tflood 12hop -104dBm -12.5dB",
                        "11s TXT_MSG direct 39B", "14s ACK     direct 1hop -88dBm 9.0dB"};
  for (int i = 0; i < 4; i++) { strcpy(mm.rows[i].text, rows[i]); mm.rows[i].tx = i == 2; mm.rows[i].snr4 = i == 1 ? 5 : 26; }
  mm.nRows = 4; strcpy(mm.radio, "910.525 MHz  SF7  BW62.5");
  clear(); monitor(d, t, mm); save(d, "deck_monitor", t, "RF Monitor");

  // airtime
  deckcore::Airtime a;
  for (int s = 0; s < 600; s += 10) {
    const float base = 250 + 200 * sinf(s * 0.02f) + (s * 7919 % 300);
    a.add(1000 + s, base, false);
    if (s % 40 == 0) a.add(1000 + s, 70, true);
  }
  AirModel am; am.a = &a; am.nowS = 1600; am.load1 = a.loadPct(1600, 6); am.load10 = a.loadPct(1600, 59); am.peak = a.peakPct();
  am.txPct10 = 0.12f; strcpy(am.radio, "20 B = 41 ms on air");
  clear(); airtime(d, t, am); save(d, "deck_airtime", t, "Airtime");

  // radar
  Blip bl[8] = {};
  const char* nm[] = {"Hilltop RPT", "Ocean View Rm", "WardenBase", "Ghent Repeater", "Larchmont", "NorfolkSensor", "Dock 12", "CBBT Relay"};
  const uint8_t ty[] = {2, 3, 1, 2, 1, 4, 1, 2};
  const double dist[] = {420, 900, 1150, 1600, 2300, 3100, 4200, 9000}, brg[] = {40, 300, 170, 95, 220, 10, 130, 60};
  for (int i = 0; i < 8; i++) {
    strcpy(bl[i].name, nm[i]); bl[i].type = ty[i]; bl[i].dist = dist[i]; bl[i].brg = brg[i];
    bl[i].eastM = dist[i] * sin(brg[i] * M_PI / 180); bl[i].northM = dist[i] * cos(brg[i] * M_PI / 180);
  }
  RadarModel rm; rm.b = bl; rm.n = 8; rm.sel = 1; rm.rangeM = deckcore::radarRange(2300, true); rm.miles = true; rm.haveMe = true; rm.sweep = 1.2f;
  clear(); radar(d, t, rm); save(d, "deck_radar", t, "Nearby");

  // trip
  deckcore::Trip tr; tr.start(0); tr.on = true; tr.have = true;
  tr.lat0 = 36.85; tr.lon0 = -76.29; tr.lat = 36.87; tr.lon = -76.26; tr.distM = 6780; tr.movingS = 1460; tr.curMps = 11.4; tr.maxMps = 17.2;
  tr.haveAlt = true; tr.alt = 7; tr.climbM = 23;
  TripModel tm; tm.t = &tr; tm.miles = true; tm.fix = true; tm.gpx = true; tm.sats = 11; tm.phase = 0.2f;
  clear(); trip(d, t, tm); save(d, "deck_trip", t, "Trip");

  // spectrum: a quiet band, LoRa bursts on the mesh channel, a carrier, a wide signal
  static speccore::Sweep sw; sw.reset();
  SpecModel sm; sm.s = &sw; sm.spanMHz = 24; sm.meshMHz = 910.525f; sm.span = speccore::makeSpan(910.525f, 24);
  for (int r = 0; r < speccore::WF_ROWS + 4; r++) {
    for (int i = 0; i < speccore::BINS; i++) {
      const float f = sm.span.freq(i);
      int v = -118 + (int)((i * 7919 + r * 104729) % 5) - 2;
      if (fabsf(f - 910.525f) < 0.1f && (r % 5 == 1 || r % 7 == 3)) v = -84 + (r % 3) * 4;
      if (fabsf(f - 915.0f) < 0.08f) v = -76;
      if (f > 902.2f && f < 903.6f && r % 9 < 4) v = -100 + (i % 3);
      sw.put(sw.pos, v); sw.next();
    }
  }
  sm.cursor = sm.span.bin(915.0f); sm.owns = true; sm.phase = 0.2f;
  clear(); spectrum(d, t, sm); save(d, "deck_spectrum", t, "Spectrum");

  // MECP inbox
  MecpItem it[3] = {};
  const char* raw[] = {"MECP/0/M01 S01 P05 3pax 36.8500,-76.2800 trail washed out", "MECP/1/T01 T04 36.9101,-76.1922",
                       "MECP/3/L07 L15"};
  const char* from[] = {"Hilltop in #hrmesh", "Ghent RPT in #757", "you in #hrmesh"};
  const char* age[] = {"2m", "41m", "3h"};
  for (int i = 0; i < 3; i++) {
    mecpcore::Decoded x; mecpcore::decode(raw[i], x);
    it[i].sev = x.sev; strcpy(it[i].words, x.words); strcpy(it[i].from, from[i]); strcpy(it[i].age, age[i]); it[i].hasPos = x.hasPos;
  }
  MecpModel mm2; mm2.items = it; mm2.n = 3; mm2.sel = 0;
  clear(); mecpInbox(d, t, mm2); save(d, "deck_mecp", t, "MECP inbox");
  return 0;
}
