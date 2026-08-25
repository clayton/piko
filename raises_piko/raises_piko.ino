#include <Arduino.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

#include "config.h"

#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12
#define LCD_WIDTH 368
#define LCD_HEIGHT 448
#define IIC_SDA 15
#define IIC_SCL 14
#define BOOT_BUTTON 0

constexpr int MAX_EVENTS = 12;
constexpr uint32_t POLL_MS = 8000;
constexpr uint32_t FRAME_MS = 100;

struct Incident {
  char id[28];
  char type[24];
  char project[25];
  char message[91];
};

Incident incidents[MAX_EVENTS]{};
int incidentCount = 0;
int selected = 0;
uint32_t revision = 0;
uint32_t lastPoll = 0;
uint32_t lastFrame = 0;
uint32_t lastReconnect = 0;
uint32_t lastButton = 0;
uint32_t arrivedAt = 0;
bool bootWasDown = false;
bool online = false;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *display = new Arduino_CO5300(
    bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT, 16, 0, 0, 0);
Arduino_Canvas canvas(LCD_WIDTH, LCD_HEIGHT, display);
XPowersPMU power;

void copyText(char *destination, size_t size, const char *source) {
  snprintf(destination, size, "%s", source ? source : "");
}

bool isInfo(const Incident &incident) {
  return !strcmp(incident.type, "notice.created");
}

bool isRegression(const Incident &incident) {
  return strstr(incident.type, "regressed") || strstr(incident.type, "reopened");
}

uint16_t moodColor() {
  if (!incidentCount) return 0x07E0;
  if (isRegression(incidents[selected])) return 0xF800;
  if (isInfo(incidents[selected])) return 0xFFE0;
  return 0xFBE0;
}

const char *moodText() {
  if (!online) return "LOOKING FOR RAISES";
  if (!incidentCount) return "ALL CLEAR";
  if (isRegression(incidents[selected])) return "OH NO, IT CAME BACK";
  if (isInfo(incidents[selected])) return "A NOTE ARRIVED";
  return "I FOUND A BUG";
}

void centered(const char *text, int y, int size, uint16_t color) {
  canvas.setTextSize(size);
  canvas.setTextColor(color);
  canvas.setCursor(max(4, (LCD_WIDTH - static_cast<int>(strlen(text)) * size * 6) / 2), y);
  canvas.print(text);
}

void drawEye(int x, int y, int dx, int dy, bool worried) {
  canvas.fillRoundRect(x - 25, y - 28, 50, 58, 18, 0xFFFF);
  canvas.fillCircle(x + dx, y + dy, 11, 0x0000);
  canvas.fillCircle(x + dx + 3, y + dy - 3, 3, 0xFFFF);
  if (worried) canvas.drawLine(x - 20, y - 34, x + 18, y - 27, 0x0000);
}

void drawPiko() {
  uint16_t color = moodColor();
  bool worried = incidentCount && !isInfo(incidents[selected]);
  int bob = sinf(millis() / 380.0f) * 3;
  int shake = isRegression(incidents[selected]) && millis() - arrivedAt < 6000 ? random(-4, 5) : 0;
  int x = 184 + shake;
  int y = 150 + bob;

  canvas.fillRoundRect(x - 116, y - 82, 232, 174, 50, color);
  canvas.fillTriangle(x - 80, y - 72, x - 58, y - 111, x - 31, y - 77, color);
  canvas.fillTriangle(x + 31, y - 77, x + 58, y - 111, x + 80, y - 72, color);

  int glance = incidentCount ? ((millis() / 900) % 3 - 1) * 5 : 0;
  drawEye(x - 52, y - 20, glance, worried ? 6 : 0, worried);
  drawEye(x + 52, y - 20, glance, worried ? 6 : 0, worried);

  if (!incidentCount) {
    canvas.drawLine(x - 20, y + 48, x, y + 58, 0x0000);
    canvas.drawLine(x, y + 58, x + 20, y + 48, 0x0000);
  } else if (isInfo(incidents[selected])) {
    canvas.fillCircle(x, y + 47, 12, 0x0000);
    canvas.fillRect(x - 12, y + 36, 24, 11, color);
  } else {
    canvas.drawCircle(x, y + 49, 15, 0x0000);
    canvas.fillRect(x - 17, y + 31, 34, 17, color);
  }
}

void wrapMessage(const char *text, int y, int maxChars, int maxLines) {
  char copy[96];
  copyText(copy, sizeof(copy), text);
  char *word = strtok(copy, " ");
  char line[40] = "";
  int lines = 0;
  while (word && lines < maxLines) {
    if (strlen(line) + strlen(word) + 1 > static_cast<size_t>(maxChars)) {
      centered(line, y + lines * 22, 2, 0xFFFF);
      ++lines;
      line[0] = '\0';
    }
    if (line[0]) strcat(line, " ");
    strncat(line, word, sizeof(line) - strlen(line) - 1);
    word = strtok(nullptr, " ");
  }
  if (line[0] && lines < maxLines) centered(line, y + lines * 22, 2, 0xFFFF);
}

void drawScreen() {
  canvas.fillScreen(0x0000);
  canvas.setTextSize(1);
  canvas.setTextColor(online ? 0x07E0 : 0x8410);
  canvas.setCursor(14, 13);
  canvas.print(online ? "RAISES IS HERE" : "CONNECTING");
  canvas.setCursor(308, 13);
  canvas.printf("%d/%d", incidentCount ? selected + 1 : 0, incidentCount);

  drawPiko();
  centered(moodText(), 249, 2, moodColor());

  if (incidentCount) {
    canvas.fillRoundRect(14, 282, 340, 132, 16, 0x1082);
    centered(incidents[selected].project, 297, 2, 0x8410);
    wrapMessage(incidents[selected].message, 329, 27, 3);
  } else {
    centered("NO NEW TROUBLE", 310, 3, 0xFFFF);
    centered("Piko is keeping watch", 360, 2, 0x8410);
  }
  canvas.flush();
}

bool fetchFeed() {
  if (WiFi.status() != WL_CONNECTED) {
    online = false;
    if (millis() - lastReconnect > 10000) {
      lastReconnect = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(1800);
  http.setTimeout(3500);
  if (!http.begin(PIKO_FEED_URL)) {
    online = false;
    return false;
  }
  http.addHeader("Authorization", "Bearer " PIKO_DEVICE_TOKEN);
  if (http.GET() != HTTP_CODE_OK) {
    http.end();
    online = false;
    return false;
  }
  JsonDocument document;
  DeserializationError error = deserializeJson(document, http.getStream());
  http.end();
  if (error) return false;

  uint32_t nextRevision = document["revision"] | 0;
  bool changed = nextRevision != revision;
  revision = nextRevision;
  online = true;
  incidentCount = 0;
  for (JsonObject event : document["events"].as<JsonArray>()) {
    if (incidentCount >= MAX_EVENTS) break;
    copyText(incidents[incidentCount].id, sizeof(incidents[incidentCount].id), event["id"] | "");
    copyText(incidents[incidentCount].type, sizeof(incidents[incidentCount].type), event["type"] | "notice.created");
    copyText(incidents[incidentCount].project, sizeof(incidents[incidentCount].project), event["project"] | "Unknown app");
    copyText(incidents[incidentCount].message, sizeof(incidents[incidentCount].message), event["message"] | "Raises event");
    ++incidentCount;
  }
  if (selected >= incidentCount) selected = 0;
  if (changed) arrivedAt = millis();
  return true;
}

void handleButtons() {
  bool bootDown = digitalRead(BOOT_BUTTON) == LOW;
  if (bootWasDown && !bootDown && millis() - lastButton > 250) {
    lastButton = millis();
    if (incidentCount) selected = (selected + 1) % incidentCount;
  }
  bootWasDown = bootDown;

  power.getIrqStatus();
  if (power.isPekeyShortPressIrq() && millis() - lastButton > 250) {
    lastButton = millis();
    power.clearIrqStatus();
    selected = 0;
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  if (power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    power.clearIrqStatus();
    power.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  }
  canvas.begin();
  display->setBrightness(155);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  drawScreen();
}

void loop() {
  handleButtons();
  if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    fetchFeed();
  }
  if (millis() - lastFrame >= FRAME_MS) {
    lastFrame = millis();
    drawScreen();
  }
  delay(15);
}
