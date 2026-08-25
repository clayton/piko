#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <Wire.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#include <esp_wifi.h>

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

constexpr uint8_t DISPLAY_BRIGHTNESS = 160;
constexpr uint32_t TOP_LONG_MS = 900;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 250;
constexpr uint32_t PMU_POLL_MS = 50;

extern Arduino_Canvas canvas;
extern XPowersPMU power;
extern volatile bool topButtonDown;
extern volatile bool topButtonPressed;
extern volatile bool topButtonLong;

void ARDUINO_ISR_ATTR topButtonInterrupt();

void hwInit();
void wifiStaReconnect();
void wifiPromiscuousBegin(wifi_promiscuous_cb_t callback, uint8_t channel);
void wifiPromiscuousEnd();
