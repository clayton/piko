#include <Arduino.h>

#include "apps.h"
#include "button_logic.h"
#include "hw.h"
#include "power_logic.h"
#include "ui.h"

void app_radar_enter();
void app_radar_exit();
void app_radar_tick();
void app_radar_draw();
const char *app_radar_status();
void app_radar_top_short();
void app_radar_bottom_short();

void app_geiger_enter();
void app_geiger_exit();
void app_geiger_tick();
void app_geiger_draw();
const char *app_geiger_status();
void app_geiger_top_short();
void app_geiger_bottom_short();

void app_raises_enter();
void app_raises_exit();
void app_raises_tick();
void app_raises_draw();
const char *app_raises_status();
void app_raises_top_short();
void app_raises_bottom_short();

const App APPS[] = {
    {"RADAR", "Who is overhead right now?", app_radar_enter, app_radar_exit, app_radar_tick, app_radar_draw,
     app_radar_status, app_radar_top_short, app_radar_bottom_short},
    {"GEIGER", "Count the invisible Wi-Fi rain", app_geiger_enter, app_geiger_exit, app_geiger_tick, app_geiger_draw,
     app_geiger_status, app_geiger_top_short, app_geiger_bottom_short},
    {"RAISES", "Bug buddy on duty", app_raises_enter, app_raises_exit, app_raises_tick, app_raises_draw,
     app_raises_status, app_raises_top_short, app_raises_bottom_short},
};

const int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);

enum RunState { STATE_APP, STATE_SWITCHER };

RunState runState = STATE_APP;
bool displayIdle = false;
int activeApp = APP_RAISES;
int switcherIndex;

TopButton topButton{};
uint32_t lastTopDebounce = 0;
uint32_t lastBottomDebounce = 0;
uint32_t lastPmuPoll = 0;
uint32_t lastMotionPoll = 0;
uint32_t lastIdleDraw = 0;
uint32_t lastActivity = 0;

void drawSwitcher() {
  canvas.fillScreen(0x0000);
  centered("PICK AN APP", 24, 3, 0x07FF);
  centered("Hold top to open, bottom to launch", 58, 1, 0x8410);

  const uint16_t colors[] = {0x07FF, 0x07E0, 0xFFE0};
  for (int i = 0; i < APP_COUNT; ++i) {
    const bool selected = i == switcherIndex;
    const int y = 92 + i * 108;
    canvas.fillRoundRect(14, y, 340, 92, 14, selected ? colors[i % 3] : 0x1082);
    canvas.setTextSize(3);
    canvas.setTextColor(selected ? 0x0000 : 0xFFFF);
    canvas.setCursor(34, y + 18);
    canvas.print(APPS[i].name);
    canvas.setTextSize(2);
    canvas.setTextColor(selected ? 0x0000 : 0xBDF7);
    canvas.setCursor(34, y + 54);
    canvas.print(APPS[i].tagline);
  }

  canvas.setTextSize(1);
  canvas.setTextColor(0x4208);
  canvas.setCursor(14, 428);
  canvas.print("TOP cycles   BOTTOM launches");
  canvas.flush();
}

void drawIdle() {
  canvas.fillScreen(0x0000);
  centered(APPS[activeApp].status(), 218, 1, 0x4208);
  canvas.flush();
}

bool wakeDisplay() {
  if (!displayIdle) return false;
  displayIdle = false;
  lastActivity = millis();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  APPS[activeApp].draw();
  return true;
}

void launchApp(int index) {
  if (index < 0 || index >= APP_COUNT) return;
  if (runState == STATE_APP && index == activeApp) return;
  if (runState == STATE_APP) APPS[activeApp].exit();
  activeApp = index;
  runState = STATE_APP;
  displayIdle = false;
  lastActivity = millis();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  APPS[activeApp].enter();
  APPS[activeApp].draw();
}

void enterSwitcher() {
  if (runState == STATE_SWITCHER) return;
  displayIdle = false;
  lastActivity = millis();
  setDisplayBrightness(DISPLAY_BRIGHTNESS);
  APPS[activeApp].exit();
  runState = STATE_SWITCHER;
  switcherIndex = activeApp;
  drawSwitcher();
}

void pollButtons() {
  const uint32_t now = millis();
  noInterrupts();
  bool interruptedPress = topButtonPressed;
  bool interruptedLong = topButtonLong;
  topButtonPressed = false;
  topButtonLong = false;
  interrupts();
  TopGesture topGesture = interruptedPress
                              ? (interruptedLong ? TOP_LONG : TOP_SHORT)
                              : updateTopButton(topButton, digitalRead(BOOT_BUTTON) == LOW, now, TOP_LONG_MS);
  if (topGesture != TOP_NONE) {
    if (wakeDisplay()) return;
    lastActivity = now;
  }
  if (topGesture == TOP_LONG && runState == STATE_APP) enterSwitcher();
  if (topGesture == TOP_SHORT && now - lastTopDebounce >= BUTTON_DEBOUNCE_MS) {
    lastTopDebounce = now;
    if (runState == STATE_SWITCHER) {
      switcherIndex = (switcherIndex + 1) % APP_COUNT;
      drawSwitcher();
    } else {
      APPS[activeApp].topShort();
    }
  }

  if (now - lastPmuPoll >= PMU_POLL_MS) {
    lastPmuPoll = now;
    power.getIrqStatus();
    if (power.isPekeyShortPressIrq()) {
      power.clearIrqStatus();
      if (wakeDisplay()) return;
      lastActivity = now;
      if (now - lastBottomDebounce >= BUTTON_DEBOUNCE_MS) {
        lastBottomDebounce = now;
        if (runState == STATE_SWITCHER) {
          launchApp(switcherIndex);
        } else {
          APPS[activeApp].bottomShort();
        }
      }
    }
  }
}

void setup() {
  randomSeed(esp_random());
  hwInit();
  topButton.wasDown = topButtonDown;
  if (topButton.wasDown) topButton.downAt = millis();
  lastPmuPoll = millis();
  lastActivity = millis();
  APPS[activeApp].enter();
  APPS[activeApp].draw();
}

void loop() {
  pollButtons();
  const uint32_t now = millis();
  if (now - lastMotionPoll >= MOTION_POLL_MS) {
    lastMotionPoll = now;
    if (moved()) {
      if (!wakeDisplay()) lastActivity = now;
    }
  }
  if (runState == STATE_APP) APPS[activeApp].tick();
  const uint32_t afterTick = millis();
  if (displayIdle && afterTick - lastIdleDraw >= 1000) {
    lastIdleDraw = afterTick;
    drawIdle();
  }
  if (shouldIdle(displayIdle, runState == STATE_APP, afterTick, lastActivity, IDLE_AFTER_MS)) {
    displayIdle = true;
    lastIdleDraw = afterTick;
    setDisplayBrightness(IDLE_BRIGHTNESS);
    drawIdle();
  }
  delay(5);
}
