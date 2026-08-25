#pragma once

struct App {
  const char *name;
  const char *tagline;
  void (*enter)();
  void (*exit)();
  void (*tick)();
  void (*draw)();
  void (*topShort)();
  void (*bottomShort)();
};

extern const App APPS[];
extern const int APP_COUNT;

enum AppId { APP_RADAR = 0, APP_GEIGER = 1, APP_RAISES = 2 };
