#pragma once

struct App {
  const char *name;
  const char *tagline;
  void (*enter)();
  void (*exit)();
  void (*tick)();
  void (*draw)();
  const char *(*status)();
  void (*topShort)();
  void (*bottomShort)();
};

extern const App APPS[];
extern const int APP_COUNT;
extern bool displayIdle;
void drawIdle();
bool wakeDisplay();
int activeAppId();
bool app_raises_dismiss();

enum AppId { APP_RADAR = 0, APP_GEIGER = 1, APP_RAISES = 2 };
