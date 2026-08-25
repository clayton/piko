#include "apps.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <math.h>

#include "config.h"
#include "hw.h"
#include "../radar/radar_logo.h"
#include "../radar/route_logic.h"
#include "ui.h"

void app_radar_draw();

namespace {

constexpr char FEED_URL[] = ADSB_FEED_URL;
constexpr char PLANE_URL[] = PLANE_DETAILS_URL;
constexpr float RANGE_NM = 60.0f;
constexpr int MAX_TARGETS = 48;
constexpr uint32_t REFRESH_MS = 1500;

struct Target {
  char callsign[9];
  char hex[7];
  char category[3];
  int altitude;
  int speed;
  int track;
  int vertRate;
  float distance;
};

struct Details {
  char hex[7];
  char callsign[9];
  char registration[12];
  char type[22];
  char model[10];
  char airline[30];
  char airlineCode[4];
  char origin[5];
  char destination[5];
};

enum Mode { MODE_NEAR, MODE_HIGH, MODE_BIG, MODE_COUNT };

Target targets[MAX_TARGETS]{};
Details details{};
int targetCount = 0;
int selectedOffset = 0;
int selectedIndex = -1;
char selectedHex[7]{};
Mode mode = MODE_NEAR;
uint32_t lastRefresh = 0;
uint32_t lastReconnect = 0;

float distanceNm(float lat, float lon) {
  const float north = (lat - RECEIVER_LAT) * 60.0f;
  const float east = (lon - RECEIVER_LON) * 60.0f * cosf(radians(RECEIVER_LAT));
  return sqrtf(north * north + east * east);
}

int categorySize(const char *category) {
  if (!category || category[0] != 'A') return 0;
  return category[1] - '0';
}

bool comesBefore(const Target &a, const Target &b) {
  if (mode == MODE_NEAR) return a.distance < b.distance;
  if (mode == MODE_HIGH) return a.altitude > b.altitude;
  const int aSize = categorySize(a.category);
  const int bSize = categorySize(b.category);
  return aSize == bSize ? a.altitude > b.altitude : aSize > bSize;
}

void selectTarget(bool keepAircraft = true) {
  if (!targetCount) {
    selectedIndex = -1;
    selectedHex[0] = '\0';
    return;
  }
  int order[MAX_TARGETS];
  for (int i = 0; i < targetCount; ++i) order[i] = i;
  for (int i = 1; i < targetCount; ++i) {
    int current = order[i];
    int j = i - 1;
    while (j >= 0 && comesBefore(targets[current], targets[order[j]])) {
      order[j + 1] = order[j];
      --j;
    }
    order[j + 1] = current;
  }
  if (keepAircraft && selectedHex[0]) {
    for (int i = 0; i < targetCount; ++i) {
      if (!strcmp(targets[order[i]].hex, selectedHex)) {
        selectedOffset = i;
        selectedIndex = order[i];
        return;
      }
    }
  }
  selectedOffset %= targetCount;
  selectedIndex = order[selectedOffset];
  copyText(selectedHex, sizeof(selectedHex), targets[selectedIndex].hex);
}

bool fetchTargets() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnect >= 10000) {
      lastReconnect = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(1200);
  http.setTimeout(1800);
  if (!http.begin(FEED_URL) || http.GET() != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  JsonDocument filter;
  for (const char *key : {"hex", "flight", "category", "lat", "lon", "altitude", "speed", "track", "vert_rate", "seen_pos"}) {
    filter["aircraft"][0][key] = true;
  }
  JsonDocument document;
  DeserializationError error = deserializeJson(document, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (error) return false;

  targetCount = 0;
  for (JsonObject aircraft : document["aircraft"].as<JsonArray>()) {
    if (targetCount >= MAX_TARGETS || !aircraft["lat"].is<float>() || !aircraft["lon"].is<float>() ||
        (aircraft["seen_pos"] | 99.0f) > 10.0f) continue;
    Target target{};
    target.distance = distanceNm(aircraft["lat"], aircraft["lon"]);
    if (target.distance > RANGE_NM) continue;
    copyText(target.callsign, sizeof(target.callsign), aircraft["flight"] | "");
    copyText(target.hex, sizeof(target.hex), aircraft["hex"] | "------");
    copyText(target.category, sizeof(target.category), aircraft["category"] | "");
    target.altitude = aircraft["altitude"] | 0;
    target.speed = aircraft["speed"] | 0;
    target.track = aircraft["track"] | 0;
    target.vertRate = aircraft["vert_rate"] | 0;
    targets[targetCount++] = target;
  }
  selectTarget();
  return true;
}

void fetchDetails(const Target &target) {
  if (!strcmp(details.hex, target.hex) && !strcmp(details.callsign, target.callsign)) return;
  details = {};
  copyText(details.hex, sizeof(details.hex), target.hex);
  copyText(details.callsign, sizeof(details.callsign), target.callsign);
  if (!target.callsign[0]) return;

  HTTPClient http;
  http.setConnectTimeout(1200);
  http.setTimeout(2000);
  String url = String(PLANE_URL) + "?hex=" + target.hex + "&callsign=" + target.callsign;
  if (!http.begin(url) || http.GET() != HTTP_CODE_OK) {
    http.end();
    return;
  }

  JsonDocument document;
  DeserializationError error = deserializeJson(document, http.getStream());
  http.end();
  if (error) return;

  JsonObject response = document["response"];
  copyText(details.registration, sizeof(details.registration), response["aircraft"]["registration"] | "");
  copyText(details.type, sizeof(details.type), response["aircraft"]["type"] | "");
  copyText(details.model, sizeof(details.model), response["aircraft"]["icao_type"] | "");
  copyText(details.airline, sizeof(details.airline), response["flightroute"]["airline"]["name"] | response["aircraft"]["registered_owner"] | "");
  copyText(details.airlineCode, sizeof(details.airlineCode), response["flightroute"]["airline"]["icao"] | "");
  JsonObject origin = response["flightroute"]["origin"];
  JsonObject destination = response["flightroute"]["destination"];
  if (origin["latitude"].is<float>() && origin["longitude"].is<float>() &&
      destination["latitude"].is<float>() && destination["longitude"].is<float>() &&
      routeNearReceiver(RECEIVER_LAT, RECEIVER_LON, origin["latitude"], origin["longitude"],
                        destination["latitude"], destination["longitude"])) {
    copyText(details.origin, sizeof(details.origin), origin["iata_code"] | "");
    copyText(details.destination, sizeof(details.destination), destination["iata_code"] | "");
  }
}


void drawModeBar() {
  const char *names[] = {"NEAR", "HIGH", "BIG"};
  constexpr int width = 112;
  for (int i = 0, x = 10; i < MODE_COUNT; x += width + 6, ++i) {
    const bool active = i == mode;
    canvas.fillRoundRect(x, 7, width, 34, 7, active ? 0x07FF : 0x1082);
    canvas.setTextSize(2);
    canvas.setTextColor(active ? 0x0000 : 0x8410);
    canvas.setCursor(x + (width - strlen(names[i]) * 12) / 2, 17);
    canvas.print(names[i]);
  }
}

void drawStat(int x, int y, const char *label, const char *value) {
  canvas.fillRoundRect(x, y, 166, 88, 10, 0x1082);
  canvas.setTextColor(0x8410);
  canvas.setTextSize(2);
  canvas.setCursor(x + 12, y + 11);
  canvas.print(label);
  canvas.setTextColor(0xFFFF);
  canvas.setTextSize(3);
  canvas.setCursor(x + 12, y + 48);
  canvas.print(value);
}

void modeNext() {
  mode = static_cast<Mode>((mode + 1) % MODE_COUNT);
  selectedOffset = 0;
  selectedHex[0] = '\0';
  details = {};
  selectTarget(false);
}

void aircraftNext() {
  if (targetCount) selectedOffset = (selectedOffset + 1) % targetCount;
  selectedHex[0] = '\0';
  details = {};
  selectTarget(false);
}

}  // namespace

void app_radar_enter() {
  targetCount = 0;
  selectedIndex = -1;
  selectedOffset = 0;
  selectedHex[0] = '\0';
  details = {};
  lastRefresh = millis();
  lastReconnect = millis();
  wifiStaReconnect();
}

void app_radar_exit() {}

void app_radar_tick() {
  if (millis() - lastRefresh >= REFRESH_MS) {
    lastRefresh = millis();
    if (fetchTargets() && selectedIndex >= 0) fetchDetails(targets[selectedIndex]);
    if (!displayIdle) app_radar_draw();
  }
}

void app_radar_draw() {
  canvas.fillScreen(0x0000);
  drawModeBar();

  if (selectedIndex < 0) {
    centered("NO AIRCRAFT", 205, 3, 0xFFFF);
    canvas.flush();
    return;
  }
  const Target &target = targets[selectedIndex];
  char logoCode[4];
  if (radarLogoCode(logoCode, details.airlineCode, target.callsign)) drawRadarLogo(canvas, logoCode, 18, 54);

  canvas.setTextColor(0x8410);
  canvas.setTextSize(1);
  canvas.setCursor(281, 59);
  canvas.printf("%d/%d", selectedOffset + 1, targetCount);
  canvas.setTextColor(0xFFFF);
  canvas.setTextSize(4);
  canvas.setCursor(121, 78);
  canvas.print(target.callsign[0] ? target.callsign : target.hex);

  const bool hasRoute = details.origin[0] && details.destination[0];
  char route[18];
  snprintf(route, sizeof(route), "%s > %s", hasRoute ? details.origin : "---",
           hasRoute ? details.destination : "---");
  centered(route, 135, 4, 0x07FF);

  char aircraft[32];
  snprintf(aircraft, sizeof(aircraft), "%s  %s", displayAircraftType(details.model), details.registration);
  centered(aircraft, 180, 2, 0xBDF7);

  char distance[14];
  char altitude[14];
  char speed[14];
  char direction[14];
  snprintf(distance, sizeof(distance), "%.1f NM", target.distance);
  if (target.altitude >= 10000) snprintf(altitude, sizeof(altitude), "%.1fK FT", target.altitude / 1000.0f);
  else snprintf(altitude, sizeof(altitude), "%d FT", target.altitude);
  snprintf(speed, sizeof(speed), "%d KT", target.speed);
  snprintf(direction, sizeof(direction), "%s", target.vertRate > 100 ? "UP" : target.vertRate < -100 ? "DOWN" : "LEVEL");

  drawStat(14, 212, "DISTANCE", distance);
  drawStat(188, 212, "ALTITUDE", altitude);
  drawStat(14, 308, "SPEED", speed);
  drawStat(188, 308, "DIRECTION", direction);

  centered(details.airline[0] ? details.airline : details.type, 418, 2, 0x8410);
  canvas.flush();
}

const char *app_radar_status() {
  static char status[32];
  if (selectedIndex < 0) return "NO AIRCRAFT";
  snprintf(status, sizeof(status), "%s  %.1f NM",
           targets[selectedIndex].callsign[0] ? targets[selectedIndex].callsign : targets[selectedIndex].hex,
           targets[selectedIndex].distance);
  return status;
}

void app_radar_top_short() {
  lastRefresh = millis();
  modeNext();
  app_radar_draw();
}

void app_radar_bottom_short() {
  lastRefresh = millis();
  aircraftNext();
  app_radar_draw();
}
