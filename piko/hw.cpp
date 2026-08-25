#include "hw.h"

#include "config.h"
#include "power_logic.h"

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *display = new Arduino_CO5300(
    bus, GFX_NOT_DEFINED, 0, LCD_WIDTH, LCD_HEIGHT, 16, 0, 0, 0);
Arduino_Canvas canvas(LCD_WIDTH, LCD_HEIGHT, display);
XPowersPMU power;
SensorQMI8658 imu;
volatile bool topButtonDown = false;
volatile bool topButtonPressed = false;
volatile bool topButtonLong = false;

void ARDUINO_ISR_ATTR topButtonInterrupt() {
  static uint32_t downAt = 0;
  bool down = digitalRead(BOOT_BUTTON) == LOW;
  uint32_t now = millis();
  if (down && !topButtonDown) downAt = now;
  if (!down && topButtonDown) {
    topButtonPressed = true;
    if (now - downAt >= TOP_LONG_MS) topButtonLong = true;
  }
  topButtonDown = down;
}

void hwInit() {
  Serial.begin(115200);
  Wire.begin(IIC_SDA, IIC_SCL);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  topButtonDown = digitalRead(BOOT_BUTTON) == LOW;
  attachInterrupt(BOOT_BUTTON, topButtonInterrupt, CHANGE);
  if (power.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    power.clearIrqStatus();
    power.setPowerKeyPressOffTime(XPOWERS_POWEROFF_6S);
    power.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  }
  canvas.begin();
  display->setBrightness(DISPLAY_BRIGHTNESS);
  if (imu.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_LOWPOWER_21Hz,
                            SensorQMI8658::LPF_MODE_0);
    imu.enableAccelerometer();
  }
}

bool moved() {
  static bool ready = false;
  static float lastX = 0;
  static float lastY = 0;
  static float lastZ = 0;
  float x, y, z;
  if (!imu.getDataReady() || !imu.getAccelerometer(x, y, z)) return false;
  bool result = ready && motionDetected(x, y, z, lastX, lastY, lastZ, WAKE_MOTION_G);
  lastX = x;
  lastY = y;
  lastZ = z;
  ready = true;
  return result;
}

void setDisplayBrightness(uint8_t brightness) {
  display->setBrightness(brightness);
}

void wifiStaReconnect() {
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  if (WiFi.status() != WL_CONNECTED) WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void wifiPromiscuousBegin(wifi_promiscuous_cb_t callback, uint8_t channel) {
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  wifi_promiscuous_filter_t filter = {
      .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(callback);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);
}

void wifiPromiscuousEnd() {
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(nullptr);
}
