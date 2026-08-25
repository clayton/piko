#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP_I2S.h>
#include <WiFi.h>
#include <Wire.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#include <esp_wifi.h>

#include "es8311.h"

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
#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8
#define SPEAKER_ENABLE 46

constexpr int HISTORY_SECONDS = 60;
constexpr int DEVICE_SLOTS = 64;
constexpr int EVENT_SLOTS = 128;
constexpr uint32_t CHANNEL_MS = 500;
constexpr uint32_t DRAW_MS = 100;

struct RadioEvent {
  uint32_t device;
  int8_t rssi;
};

struct SeenDevice {
  uint32_t hash;
  uint32_t seenAt;
};

volatile RadioEvent events[EVENT_SLOTS]{};
volatile uint16_t eventHead = 0;
volatile uint16_t eventTail = 0;
volatile uint32_t rawPackets = 0;
volatile uint8_t sampleDivisor = 16;
portMUX_TYPE eventMux = portMUX_INITIALIZER_UNLOCKED;

uint16_t history[HISTORY_SECONDS]{};
SeenDevice devices[DEVICE_SLOTS]{};
uint8_t historyIndex = 0;
uint8_t channel = 1;
uint8_t sensitivity = 1;
uint32_t lastSecond = 0;
uint32_t lastChannel = 0;
uint32_t lastDraw = 0;
uint32_t lastClick = 0;
uint32_t lastButton = 0;
uint32_t latestEvent = 0;
uint32_t packetsSnapshot = 0;
uint16_t currentEvents = 0;
int8_t latestRssi = -100;
bool bootWasDown = false;
bool soundOn = true;

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *display = new Arduino_CO5300(
    bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT, 16, 0, 0, 0);
Arduino_Canvas canvas(LCD_WIDTH, LCD_HEIGHT, display);
XPowersPMU power;
I2SClass i2s;

uint32_t deviceHash(const uint8_t *address) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < 6; ++i) hash = (hash ^ address[i]) * 16777619u;
  return hash ?: 1;
}

void radioPacket(void *buffer, wifi_promiscuous_pkt_type_t type) {
  if (type == WIFI_PKT_MISC) return;
  auto *packet = static_cast<wifi_promiscuous_pkt_t *>(buffer);
  if (packet->rx_ctrl.rssi < -88 || packet->rx_ctrl.sig_len < 16) return;

  portENTER_CRITICAL_ISR(&eventMux);
  uint32_t packetNumber = ++rawPackets;
  if (packetNumber % sampleDivisor == 0) {
    uint16_t next = (eventHead + 1) % EVENT_SLOTS;
    if (next != eventTail) {
      events[eventHead].device = deviceHash(packet->payload + 10);
      events[eventHead].rssi = packet->rx_ctrl.rssi;
      eventHead = next;
    }
  }
  portEXIT_CRITICAL_ISR(&eventMux);
}

bool popEvent(RadioEvent &event) {
  bool found = false;
  portENTER_CRITICAL(&eventMux);
  if (eventTail != eventHead) {
    event.device = events[eventTail].device;
    event.rssi = events[eventTail].rssi;
    eventTail = (eventTail + 1) % EVENT_SLOTS;
    found = true;
  }
  portEXIT_CRITICAL(&eventMux);
  return found;
}

void rememberDevice(uint32_t hash) {
  uint32_t oldest = UINT32_MAX;
  int slot = 0;
  for (int i = 0; i < DEVICE_SLOTS; ++i) {
    if (devices[i].hash == hash) {
      devices[i].seenAt = millis();
      return;
    }
    if (devices[i].seenAt < oldest) {
      oldest = devices[i].seenAt;
      slot = i;
    }
  }
  devices[slot] = {hash, millis()};
}

int activeDevices() {
  int count = 0;
  for (const SeenDevice &device : devices)
    if (device.hash && millis() - device.seenAt < 60000) ++count;
  return count;
}

void click() {
  if (!soundOn || millis() - lastClick < 35) return;
  lastClick = millis();
  int16_t samples[96]{};
  for (int i = 0; i < 48; ++i) {
    int16_t value = i < 4 ? (i & 1 ? -10000 : 10000) : 0;
    samples[i * 2] = value;
    samples[i * 2 + 1] = value;
  }
  i2s.write(reinterpret_cast<uint8_t *>(samples), sizeof(samples));
}

void initAudio() {
  pinMode(SPEAKER_ENABLE, OUTPUT);
  digitalWrite(SPEAKER_ENABLE, HIGH);
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) return;
  es8311_handle_t codec = es8311_create(0, ES8311_ADDRESS_0);
  const es8311_clock_config_t clock = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency = 16000 * 256,
      .sample_frequency = 16000};
  if (codec && es8311_init(codec, &clock, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) == ESP_OK)
    es8311_voice_volume_set(codec, 55, nullptr);
}

int eventsPerMinute() {
  int total = currentEvents;
  for (uint16_t value : history) total += value;
  return total;
}

uint16_t activityColor(int cpm) {
  if (cpm < 30) return 0x07FF;
  if (cpm < 100) return 0x07E0;
  if (cpm < 250) return 0xFFE0;
  return 0xF800;
}

const char *activityName(int cpm) {
  if (cpm < 30) return "QUIET";
  if (cpm < 100) return "ACTIVE";
  if (cpm < 250) return "HOT";
  return "CHAOS";
}

void centered(const char *text, int y, int size, uint16_t color) {
  canvas.setTextSize(size);
  canvas.setTextColor(color);
  canvas.setCursor(max(4, (LCD_WIDTH - static_cast<int>(strlen(text)) * 6 * size) / 2), y);
  canvas.print(text);
}

void drawScreen() {
  int cpm = eventsPerMinute();
  uint16_t color = activityColor(cpm);
  canvas.fillScreen(0x0000);

  const char *levels[] = {"LOW", "MED", "HIGH"};
  for (int i = 0; i < 3; ++i) {
    int x = 10 + i * 118;
    canvas.fillRoundRect(x, 8, 112, 32, 7, i == sensitivity ? color : 0x1082);
    canvas.setTextSize(2);
    canvas.setTextColor(i == sensitivity ? 0x0000 : 0x8410);
    canvas.setCursor(x + (112 - strlen(levels[i]) * 12) / 2, 17);
    canvas.print(levels[i]);
  }

  centered("RADIOACTIVITY", 62, 2, 0x8410);
  char score[8];
  snprintf(score, sizeof(score), "%d", cpm);
  centered(score, 91, cpm < 1000 ? 7 : 6, 0xFFFF);
  centered("COUNTS / MIN", 154, 2, 0x8410);
  centered(activityName(cpm), 187, 4, color);

  canvas.fillRoundRect(14, 239, 340, 90, 10, 0x0841);
  int peak = 1;
  for (uint16_t value : history) peak = max(peak, static_cast<int>(value));
  peak = max(peak, static_cast<int>(currentEvents));
  for (int i = 0; i < HISTORY_SECONDS; ++i) {
    int index = (historyIndex + i + 1) % HISTORY_SECONDS;
    int height = history[index] * 70 / peak;
    canvas.fillRect(22 + i * 5, 319 - height, 3, height, color);
  }
  int currentHeight = currentEvents * 70 / peak;
  canvas.fillRect(322, 319 - currentHeight, 3, currentHeight, 0xFFFF);

  canvas.setTextSize(1);
  canvas.setTextColor(0x8410);
  canvas.setCursor(18, 349); canvas.print("DEVICES");
  canvas.setCursor(140, 349); canvas.print("CHANNEL");
  canvas.setCursor(264, 349); canvas.print(soundOn ? "SOUND" : "MUTED");
  canvas.setTextSize(3);
  canvas.setTextColor(0xFFFF);
  canvas.setCursor(18, 370); canvas.printf("%d", activeDevices());
  canvas.setCursor(140, 370); canvas.printf("%d", channel);
  canvas.setCursor(264, 370); canvas.print(soundOn ? "ON" : "OFF");

  if (millis() - latestEvent < 120) {
    int radius = map(constrain(latestRssi, -88, -30), -88, -30, 5, 18);
    canvas.fillCircle(334, 431, radius, color);
  }
  canvas.setTextColor(0x4208);
  canvas.setTextSize(1);
  canvas.setCursor(14, 427);
  canvas.printf("%lu PACKETS", static_cast<unsigned long>(packetsSnapshot));
  canvas.flush();
}

void handleButtons() {
  bool bootDown = digitalRead(BOOT_BUTTON) == LOW;
  if (bootWasDown && !bootDown && millis() - lastButton > 250) {
    lastButton = millis();
    sensitivity = (sensitivity + 1) % 3;
    const uint8_t divisors[] = {32, 16, 8};
    sampleDivisor = divisors[sensitivity];
    memset(history, 0, sizeof(history));
    currentEvents = 0;
  }
  bootWasDown = bootDown;

  power.getIrqStatus();
  if (power.isPekeyShortPressIrq() && millis() - lastButton > 250) {
    lastButton = millis();
    power.clearIrqStatus();
    soundOn = !soundOn;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  if (power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    power.clearIrqStatus();
    power.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  }
  canvas.begin();
  display->setBrightness(150);
  initAudio();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(radioPacket);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);
  drawScreen();
}

void loop() {
  handleButtons();

  RadioEvent event;
  while (popEvent(event)) {
    ++currentEvents;
    latestEvent = millis();
    latestRssi = event.rssi;
    rememberDevice(event.device);
    click();
  }

  if (millis() - lastChannel >= CHANNEL_MS) {
    lastChannel = millis();
    channel = channel % 11 + 1;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  }

  if (millis() - lastSecond >= 1000) {
    lastSecond += 1000;
    historyIndex = (historyIndex + 1) % HISTORY_SECONDS;
    history[historyIndex] = currentEvents;
    currentEvents = 0;
    portENTER_CRITICAL(&eventMux);
    packetsSnapshot = rawPackets;
    portEXIT_CRITICAL(&eventMux);
  }

  if (millis() - lastDraw >= DRAW_MS) {
    lastDraw = millis();
    drawScreen();
  }
  delay(5);
}
