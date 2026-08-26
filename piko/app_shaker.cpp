#include "apps.h"

#include <ESP_I2S.h>

#include "es8311.h"
#include "hw.h"
#include "shaker_logic.h"
#include "ui.h"

void app_shaker_draw();

namespace {

constexpr int PARTICLE_COUNT = 60;
constexpr int LEFT = 8;
constexpr int TOP = 8;
constexpr int RIGHT = LCD_WIDTH - 8;
constexpr int BOTTOM = LCD_HEIGHT - 8;
constexpr float PARTICLE_RADIUS = 3;
constexpr float GRAVITY = 520;
constexpr uint32_t FRAME_MS = 33;
constexpr uint16_t PARTICLE_COLORS[] = {0x07FF, 0xFFE0, 0xF81F, 0x07E0, 0xFFFF};

ShakerParticle particles[PARTICLE_COUNT]{};
I2SClass i2s;
bool audioReady = false;
bool squashed = false;
uint32_t lastFrame = 0;
uint32_t lastClick = 0;
float pendingShake = 0;

void sound(int frequency, int volume = 5000) {
  if (!audioReady) return;
  int16_t samples[64 * 2];
  for (int i = 0; i < 64; ++i) {
    int16_t value = sinf(2 * PI * frequency * i / 16000.0f) * volume * (64 - i) / 64;
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
  audioReady = codec && es8311_init(codec, &clock, ES8311_RESOLUTION_16,
                                    ES8311_RESOLUTION_16) == ESP_OK;
  if (audioReady) es8311_voice_volume_set(codec, 45, nullptr);
  if (codec) es8311_delete(codec);
}

void resetParticles() {
  for (auto &particle : particles) {
    particle.x = random(LEFT + 8, RIGHT - 8);
    particle.y = random(TOP + 8, BOTTOM - 8);
    particle.vx = random(-30, 31);
    particle.vy = random(-30, 31);
  }
  squashed = false;
  lastFrame = millis();
  lastClick = 0;
  pendingShake = 0;
}

void drawPiko() {
  int width = squashed ? 214 : 176;
  int height = squashed ? 106 : 150;
  int x = LCD_WIDTH / 2;
  int y = squashed ? 318 : 294;
  int left = x - width / 2;
  int top = y - height / 2;
  uint16_t color = squashed ? 0xF81F : 0x07FF;
  canvas.fillRoundRect(left, top, width, height, 42, color);
  canvas.fillTriangle(left + 27, top + 15, left + 52, top - 24,
                      left + 72, top + 15, color);
  canvas.fillTriangle(left + width - 72, top + 15, left + width - 52, top - 24,
                      left + width - 27, top + 15, color);
  int eyeY = y - (squashed ? 12 : 24);
  int eyeDx = constrain(int(particles[0].x - x) / 25, -5, 5);
  for (int eyeX : {x - 42, x + 42}) {
    canvas.fillRoundRect(eyeX - 20, eyeY - 23, 40, 48, 15, 0xFFFF);
    canvas.fillCircle(eyeX + eyeDx, eyeY, 9, 0x0000);
    canvas.fillCircle(eyeX + eyeDx + 2, eyeY - 2, 2, 0xFFFF);
  }
  canvas.drawLine(x - 17, y + 36, x, y + 44, 0x0000);
  canvas.drawLine(x, y + 44, x + 17, y + 36, 0x0000);
}

}  // namespace

void app_shaker_enter() {
  resetParticles();
  initAudio();
}

void app_shaker_exit() {
  i2s.end();
  audioReady = false;
  digitalWrite(SPEAKER_ENABLE, LOW);
}

void app_shaker_tick() {
  uint32_t now = millis();
  if (displayIdle || now - lastFrame < FRAME_MS) return;
  float dt = min(now - lastFrame, uint32_t(50)) / 1000.0f;
  lastFrame = now;

  float ax, ay, az;
  acceleration(ax, ay, az);
  float gx = ax * GRAVITY;
  float gy = ay * GRAVITY;
  if (pendingShake >= SHAKE_MOTION_G) {
    float energy = min(1.0f, pendingShake / 2.5f);
    for (auto &particle : particles) {
      particle.vx += random(-180, 181) * energy;
      particle.vy += random(-180, 181) * energy;
    }
  }
  pendingShake = 0;

  bool nextSquashed = topButtonDown;
  if (nextSquashed != squashed) {
    squashed = nextSquashed;
    sound(squashed ? 180 : 320, 4500);
  }

  bool hit = false;
  for (auto &particle : particles)
    hit |= stepShakerParticle(particle, gx, gy, dt, LEFT, TOP, RIGHT, BOTTOM, PARTICLE_RADIUS);
  if (hit && now - lastClick > 90) {
    lastClick = now;
    sound(900 + random(0, 500), 1800);
  }
  app_shaker_draw();
}

void app_shaker_motion(float motion) {
  pendingShake = max(pendingShake, motion);
}

void app_shaker_draw() {
  canvas.fillScreen(0x0000);
  canvas.drawRoundRect(LEFT, TOP, RIGHT - LEFT, BOTTOM - TOP, 18, 0x2104);
  for (int i = 0; i < PARTICLE_COUNT; ++i)
    canvas.fillCircle(roundf(particles[i].x), roundf(particles[i].y), PARTICLE_RADIUS,
                      PARTICLE_COLORS[i % 5]);
  drawPiko();
  centered("TILT  SHAKE  SQUISH", 410, 1, 0x8410);
  canvas.flush();
}

const char *app_shaker_status() {
  return "PIKO IS SETTLING";
}

void app_shaker_top_short() {}

void app_shaker_bottom_short() {
  for (auto &particle : particles) {
    particle.vx += random(-120, 121);
    particle.vy -= random(80, 220);
  }
  sound(240, 3500);
}
