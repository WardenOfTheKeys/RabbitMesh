// RabbitMesh: the time zone from where the GPS says you are, for North America (the US,
// Canada, Mexico) - the region RabbitMesh is built for. Kept apart from the clock so the host
// tests can check it against real towns (test/tz_core_test.cpp).
//
// The zone lines are drawn as polylines along their real courses (state and province lines,
// and the county lines where a state is split: the Idaho panhandle, eastern Oregon, the
// Dakotas, Nebraska and Kansas, west Texas, Indiana, Kentucky, Tennessee, the Florida
// panhandle, Michigan's Upper Peninsula, northwestern Ontario). Within a few km of a line it
// can pick the neighbour; the owner can then set the zone by hand, which turns this off.
// Outside North America it answers Z_UNKNOWN and the zone is left as it was.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace tzcore {

// The zones, by what the T-Deck's zone list calls them (regional.cpp ZONES).
enum Zone : uint8_t {
  Z_UNKNOWN = 0,
  Z_HAWAII, Z_ALASKA, Z_PACIFIC, Z_ARIZONA,        // Arizona: UTC-7 all year (also Sonora, Yukon)
  Z_MOUNTAIN, Z_CENTRAL, Z_CENTRAL_NODST,          // no DST: Saskatchewan, most of Mexico
  Z_EASTERN, Z_ATLANTIC, Z_NEWFOUNDLAND,
};
// How regional.cpp's ZONES names begin, to find each one there.
inline const char* zonePrefix(uint8_t z) {
  switch (z) {
    case Z_HAWAII: return "Hawaii";
    case Z_ALASKA: return "Alaska";
    case Z_PACIFIC: return "Pacific";
    case Z_ARIZONA: return "Arizona";
    case Z_MOUNTAIN: return "Mountain";
    case Z_CENTRAL: return "Central";
    case Z_CENTRAL_NODST: return "Mexico City";
    case Z_EASTERN: return "Eastern";
    case Z_ATLANTIC: return "Atlantic";
    case Z_NEWFOUNDLAND: return "Newfoundland";
    default: return nullptr;
  }
}

struct Pt { float a, b; };       // a polyline point: (latitude, longitude) or (longitude, latitude)

// Piecewise-linear y for x along points sorted by x; clamped at the ends.
inline float along(const Pt* p, size_t n, float x) {
  if (x <= p[0].a) return p[0].b;
  for (size_t i = 1; i < n; i++)
    if (x <= p[i].a) {
      const float f = (x - p[i - 1].a) / (p[i].a - p[i - 1].a);
      return p[i - 1].b + f * (p[i].b - p[i - 1].b);
    }
  return p[n - 1].b;
}
#define TZ_ALONG(arr, x) along(arr, sizeof(arr) / sizeof(arr[0]), x)

// ---- the lines (latitude -> the longitude of the line) ------------------------------------------
// Pacific | Mountain, from Mexico to the Yukon: the Colorado river, the Nevada-Utah line, eastern
// Oregon (Malheur County is Mountain), the Salmon river across Idaho, the Idaho-Montana line, then
// the Continental Divide (BC-Alberta).
static const Pt PAC_MTN[] = {
  {31.3f, -114.8f}, {35.1f, -114.6f}, {36.0f, -114.5f}, {37.0f, -114.05f}, {42.0f, -114.05f},
  {42.0f, -118.2f}, {44.5f, -118.2f}, {44.6f, -117.0f}, {45.4f, -116.6f}, {45.6f, -114.5f},
  {46.6f, -114.6f}, {47.5f, -115.7f}, {49.0f, -116.0f}, {49.1f, -116.9f}, {50.5f, -117.3f},
  {51.5f, -117.6f}, {52.5f, -118.5f}, {53.8f, -120.0f}, {60.0f, -120.0f}};
// Mountain | Central: west Texas (El Paso and Hudspeth are Mountain), the Texas-New Mexico and
// Oklahoma lines, western Kansas, the Nebraska panhandle, the Missouri river across the
// Dakotas, then (Canada) the Saskatchewan-Alberta line.
static const Pt MTN_CEN[] = {
  {29.0f, -104.6f}, {31.0f, -104.9f}, {31.9f, -104.9f}, {32.0f, -103.05f}, {36.5f, -103.05f},
  {37.0f, -101.55f}, {40.0f, -101.55f}, {40.0f, -101.4f}, {43.0f, -101.3f}, {43.0f, -100.6f},
  {45.9f, -100.6f}, {46.0f, -101.3f}, {47.4f, -102.0f}, {47.6f, -103.6f}, {49.0f, -104.0f},
  {49.01f, -110.0f}, {60.0f, -110.0f}};
// Central | Eastern: the Apalachicola in Florida, the Georgia-Alabama line, across Tennessee and
// Kentucky on the county lines, Indiana's two Central corners, Lake Michigan, the Upper Peninsula's
// Wisconsin-border counties, then (Canada) through northwestern Ontario.
static const Pt CEN_EAS[] = {
  {29.5f, -85.1f}, {31.0f, -85.0f}, {32.0f, -85.05f}, {35.0f, -85.5f}, {35.4f, -85.2f},
  {35.95f, -84.85f}, {36.6f, -85.0f}, {37.1f, -85.25f}, {37.7f, -86.0f}, {38.0f, -86.7f},
  {38.3f, -87.35f}, {41.0f, -87.53f}, {41.2f, -86.55f}, {41.8f, -86.55f}, {41.9f, -87.3f},
  {45.0f, -87.3f}, {45.8f, -87.65f}, {47.5f, -88.3f}, {48.2f, -89.5f}, {49.0f, -90.0f},
  {51.0f, -90.0f}, {56.0f, -89.0f}};
// The US-Mexico border (longitude -> latitude): the land line west, then the Rio Grande.
static const Pt MEX_BORDER[] = {
  {-117.1f, 32.53f}, {-114.7f, 32.72f}, {-111.07f, 31.33f}, {-108.2f, 31.33f}, {-108.2f, 31.78f},
  {-106.53f, 31.78f}, {-106.49f, 31.745f}, {-106.40f, 31.70f}, {-106.2f, 31.47f}, {-105.6f, 31.0f},
  {-104.7f, 29.8f}, {-103.3f, 29.0f}, {-101.2f, 29.6f}, {-100.5f, 28.7f}, {-99.5f, 27.5f},
  {-99.0f, 26.4f}, {-97.5f, 25.85f}, {-97.15f, 25.95f}};
// Since 2022 most of Mexico keeps UTC-6 all year, but the towns along the US border keep their
// clocks with the US side (Tijuana Pacific, Ciudad Juarez Mountain, Matamoros Central, with
// daylight saving; the Sonora border Arizona-like): a strip about 30 km deep.
constexpr float BORDER_STRIP_DEG = 0.27f;

// south of the border; strip: within the border strip (keeps the US side's time)
inline bool inMexico(float lat, float lon, bool* strip = nullptr) {
  if (strip) *strip = false;
  if (lon < -117.2f || lon > -86.5f || lat > 32.8f || lat < 14.5f) return false;
  if (lon > -97.15f) return lat < 25.95f && lon < -86.7f;   // the Gulf coast south of Texas, the Yucatan
  const float b = TZ_ALONG(MEX_BORDER, lon);
  if (lat >= b) return false;
  if (strip) *strip = lat > b - BORDER_STRIP_DEG;
  return true;
}

inline uint8_t zoneFor(float lat, float lon) {
  // the far corners first
  if (lat > 18.5f && lat < 22.5f && lon < -154.5f && lon > -160.8f) return Z_HAWAII;
  if (lat > 51.0f && lon < -141.0f) return Z_ALASKA;                     // Alaska (the Aleutians too)
  if (lat > 54.6f && lon < -130.0f && lon >= -141.0f && lat < 60.0f) return Z_ALASKA;   // the panhandle
  if (lat >= 60.0f && lon >= -141.0f && lon < -124.0f) return Z_ARIZONA; // Yukon: UTC-7 all year
  if (lat < 14.5f || lat > 72.0f || lon < -170.0f || lon > -50.0f) return Z_UNKNOWN;

  bool strip = false;
  if (inMexico(lat, lon, &strip)) {
    if (lon < -114.8f && lat > 28.0f) return Z_PACIFIC;                 // Baja California
    if (strip && lon >= -108.5f) {                                       // the border towns: the US side's zone
      if (lon < TZ_ALONG(MTN_CEN, lat)) return Z_MOUNTAIN;
      return Z_CENTRAL;
    }
    if (lon < -108.5f && lat > 26.3f) return Z_ARIZONA;                 // Sonora: UTC-7 all year
    if (lat > 21.0f && lat < 27.0f && lon < -104.5f - (lat - 21.0f) * 0.65f) return Z_ARIZONA;   // Sinaloa, Nayarit (UTC-7), west of the Sierra
    return Z_CENTRAL_NODST;                                              // most of Mexico: UTC-6 all year
  }
  // Newfoundland (the island) and Atlantic Canada
  if (lat > 46.5f && lat < 51.8f && lon > -59.5f) return Z_NEWFOUNDLAND;
  if (lat >= 48.2f && lon > -63.3f) return Z_ATLANTIC;                  // Labrador, the far Quebec coast
  if (lat > 43.3f && lat < 48.2f && lon > -66.9f) return Z_ATLANTIC;    // NB, NS, PEI (Maine stays Eastern)
  if (lat > 45.6f && lat < 48.2f && lon > -68.3f && lon > -69.0f + (lat - 45.6f) * 0.27f && lat > 47.0f) return Z_ATLANTIC;
  // Arizona (the state keeps UTC-7 all year), west of New Mexico
  if (lat > 31.3f && lat < 37.0f && lon > -114.6f && lon < -109.05f) return Z_ARIZONA;
  // Saskatchewan: Central time all year
  if (lat >= 49.0f && lat < 60.0f && lon > -110.0f && lon < -101.4f) return Z_CENTRAL_NODST;

  if (lon < TZ_ALONG(PAC_MTN, lat)) return Z_PACIFIC;
  if (lon < TZ_ALONG(MTN_CEN, lat)) return Z_MOUNTAIN;
  if (lon < TZ_ALONG(CEN_EAS, lat)) return Z_CENTRAL;
  return Z_EASTERN;
}

}  // namespace tzcore
