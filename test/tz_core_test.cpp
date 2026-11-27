// Host tests for the time zone from location (src/tz_core.h), on real towns either side of the
// zone lines.
#include <cstdio>
#include "tz_core.h"
using namespace tzcore;
static int fails = 0;
static const char* N[] = {"unknown", "Hawaii", "Alaska", "Pacific", "Arizona", "Mountain", "Central", "Central(no DST)",
                          "Eastern", "Atlantic", "Newfoundland"};

static void town(const char* name, float lat, float lon, uint8_t want) {
  const uint8_t got = zoneFor(lat, lon);
  if (got != want) { printf("FAIL %-26s %8.3f %9.3f  got %s, want %s\n", name, lat, lon, N[got], N[want]); fails++; }
}

int main() {
  town("Honolulu", 21.31f, -157.86f, Z_HAWAII);
  town("Anchorage", 61.22f, -149.90f, Z_ALASKA);
  town("Juneau", 58.30f, -134.42f, Z_ALASKA);
  town("Whitehorse", 60.72f, -135.06f, Z_ARIZONA);
  town("Seattle", 47.61f, -122.33f, Z_PACIFIC);
  town("Spokane", 47.66f, -117.43f, Z_PACIFIC);
  town("Coeur d'Alene ID", 47.68f, -116.78f, Z_PACIFIC);
  town("Lewiston ID", 46.42f, -117.02f, Z_PACIFIC);
  town("Boise ID", 43.62f, -116.20f, Z_MOUNTAIN);
  town("Ontario OR", 44.03f, -116.96f, Z_MOUNTAIN);
  town("Bend OR", 44.06f, -121.31f, Z_PACIFIC);
  town("Missoula MT", 46.87f, -113.99f, Z_MOUNTAIN);
  town("Portland OR", 45.52f, -122.68f, Z_PACIFIC);
  town("San Francisco", 37.77f, -122.42f, Z_PACIFIC);
  town("Los Angeles", 34.05f, -118.24f, Z_PACIFIC);
  town("San Diego", 32.72f, -117.16f, Z_PACIFIC);
  town("Las Vegas", 36.17f, -115.14f, Z_PACIFIC);
  town("Reno", 39.53f, -119.81f, Z_PACIFIC);
  town("Salt Lake City", 40.76f, -111.89f, Z_MOUNTAIN);
  town("Phoenix", 33.45f, -112.07f, Z_ARIZONA);
  town("Tucson", 32.22f, -110.97f, Z_ARIZONA);
  town("Flagstaff", 35.20f, -111.65f, Z_ARIZONA);
  town("Albuquerque", 35.08f, -106.65f, Z_MOUNTAIN);
  town("Denver", 39.74f, -104.99f, Z_MOUNTAIN);
  town("El Paso", 31.76f, -106.49f, Z_MOUNTAIN);
  town("Midland TX", 31.99f, -102.08f, Z_CENTRAL);
  town("Amarillo", 35.22f, -101.83f, Z_CENTRAL);
  town("Dallas", 32.78f, -96.80f, Z_CENTRAL);
  town("Houston", 29.76f, -95.37f, Z_CENTRAL);
  town("Brownsville TX", 25.90f, -97.50f, Z_CENTRAL);
  town("Laredo TX", 27.53f, -99.49f, Z_CENTRAL);
  town("Goodland KS", 39.35f, -101.71f, Z_MOUNTAIN);
  town("Dodge City KS", 37.75f, -100.02f, Z_CENTRAL);
  town("Scottsbluff NE", 41.87f, -103.66f, Z_MOUNTAIN);
  town("North Platte NE", 41.12f, -100.77f, Z_CENTRAL);
  town("Rapid City SD", 44.08f, -103.23f, Z_MOUNTAIN);
  town("Pierre SD", 44.37f, -100.35f, Z_CENTRAL);
  town("Bismarck ND", 46.81f, -100.78f, Z_CENTRAL);
  town("Dickinson ND", 46.88f, -102.79f, Z_MOUNTAIN);
  town("Williston ND", 48.15f, -103.62f, Z_CENTRAL);
  town("Fargo ND", 46.88f, -96.79f, Z_CENTRAL);
  town("Minneapolis", 44.98f, -93.27f, Z_CENTRAL);
  town("Chicago", 41.88f, -87.63f, Z_CENTRAL);
  town("Milwaukee", 43.04f, -87.91f, Z_CENTRAL);
  town("Gary IN", 41.59f, -87.35f, Z_CENTRAL);
  town("South Bend IN", 41.68f, -86.25f, Z_EASTERN);
  town("Indianapolis", 39.77f, -86.16f, Z_EASTERN);
  town("Terre Haute IN", 39.47f, -87.41f, Z_EASTERN);
  town("Evansville IN", 37.97f, -87.57f, Z_CENTRAL);
  town("Louisville KY", 38.25f, -85.76f, Z_EASTERN);
  town("Elizabethtown KY", 37.69f, -85.86f, Z_EASTERN);
  town("Bowling Green KY", 36.99f, -86.44f, Z_CENTRAL);
  town("Lexington KY", 38.04f, -84.50f, Z_EASTERN);
  town("Nashville", 36.16f, -86.78f, Z_CENTRAL);
  town("Crossville TN", 35.95f, -85.03f, Z_CENTRAL);
  town("Knoxville", 35.96f, -83.92f, Z_EASTERN);
  town("Chattanooga", 35.05f, -85.31f, Z_EASTERN);
  town("Birmingham AL", 33.52f, -86.80f, Z_CENTRAL);
  town("Atlanta", 33.75f, -84.39f, Z_EASTERN);
  town("Columbus GA", 32.46f, -84.99f, Z_EASTERN);
  town("Pensacola FL", 30.42f, -87.22f, Z_CENTRAL);
  town("Panama City FL", 30.16f, -85.66f, Z_CENTRAL);
  town("Tallahassee", 30.44f, -84.28f, Z_EASTERN);
  town("Miami", 25.76f, -80.19f, Z_EASTERN);
  town("Detroit", 42.33f, -83.05f, Z_EASTERN);
  town("Grand Rapids MI", 42.96f, -85.67f, Z_EASTERN);
  town("Marquette MI", 46.54f, -87.40f, Z_EASTERN);
  town("Iron Mountain MI", 45.82f, -88.07f, Z_CENTRAL);
  town("Norfolk VA", 36.85f, -76.29f, Z_EASTERN);
  town("New York", 40.71f, -74.01f, Z_EASTERN);
  town("Boston", 42.36f, -71.06f, Z_EASTERN);
  town("Bangor ME", 44.80f, -68.77f, Z_EASTERN);
  town("Caribou ME", 46.86f, -68.01f, Z_EASTERN);
  town("Vancouver", 49.28f, -123.12f, Z_PACIFIC);
  town("Cranbrook BC", 49.51f, -115.77f, Z_MOUNTAIN);
  town("Calgary", 51.05f, -114.07f, Z_MOUNTAIN);
  town("Edmonton", 53.55f, -113.49f, Z_MOUNTAIN);
  town("Regina", 50.45f, -104.61f, Z_CENTRAL_NODST);
  town("Saskatoon", 52.13f, -106.67f, Z_CENTRAL_NODST);
  town("Winnipeg", 49.90f, -97.14f, Z_CENTRAL);
  town("Kenora ON", 49.77f, -94.49f, Z_CENTRAL);
  town("Thunder Bay ON", 48.38f, -89.25f, Z_EASTERN);
  town("Toronto", 43.65f, -79.38f, Z_EASTERN);
  town("Ottawa", 45.42f, -75.70f, Z_EASTERN);
  town("Montreal", 45.50f, -73.57f, Z_EASTERN);
  town("Quebec City", 46.81f, -71.21f, Z_EASTERN);
  town("Gaspe QC", 48.83f, -64.48f, Z_EASTERN);
  town("Moncton NB", 46.09f, -64.78f, Z_ATLANTIC);
  town("Saint John NB", 45.27f, -66.06f, Z_ATLANTIC);
  town("Halifax", 44.65f, -63.57f, Z_ATLANTIC);
  town("Charlottetown", 46.24f, -63.13f, Z_ATLANTIC);
  town("St. John's NL", 47.56f, -52.71f, Z_NEWFOUNDLAND);
  town("Corner Brook NL", 48.95f, -57.95f, Z_NEWFOUNDLAND);
  town("Goose Bay NL", 53.30f, -60.33f, Z_ATLANTIC);
  town("Tijuana", 32.51f, -117.04f, Z_PACIFIC);
  town("Hermosillo", 29.07f, -110.96f, Z_ARIZONA);
  town("Ciudad Juarez (border: US-aligned)", 31.69f, -106.42f, Z_MOUNTAIN);
  town("Matamoros (border)", 25.87f, -97.50f, Z_CENTRAL);
  town("Nogales Sonora (UTC-7)", 31.30f, -110.94f, Z_ARIZONA);
  town("Chihuahua city", 28.63f, -106.07f, Z_CENTRAL_NODST);
  town("Mazatlan (UTC-7)", 23.25f, -106.41f, Z_ARIZONA);
  town("Culiacan (UTC-7)", 24.81f, -107.39f, Z_ARIZONA);
  town("Tepic (UTC-7)", 21.50f, -104.89f, Z_ARIZONA);
  town("Durango", 24.03f, -104.67f, Z_CENTRAL_NODST);
  town("Monterrey", 25.69f, -100.32f, Z_CENTRAL_NODST);
  town("Mexico City", 19.43f, -99.13f, Z_CENTRAL_NODST);
  town("Guadalajara", 20.67f, -103.35f, Z_CENTRAL_NODST);
  town("London (outside)", 51.51f, -0.13f, Z_UNKNOWN);
  town("Sydney (outside)", -33.87f, 151.21f, Z_UNKNOWN);
  town("Gulf of Guinea (0,0)", 0.0f, 0.0f, Z_UNKNOWN);
  for (int z = Z_HAWAII; z <= Z_NEWFOUNDLAND; z++) if (!zonePrefix(z)) { printf("FAIL no prefix for %d\n", z); fails++; }

  printf(fails ? "%d FAILED\n" : "all tz_core tests passed\n", fails);
  return fails != 0;
}
