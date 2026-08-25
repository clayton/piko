#pragma once

#include <stdint.h>

struct TopButton {
  bool wasDown;
  bool longFired;
  uint32_t downAt;
};

enum TopGesture { TOP_NONE, TOP_SHORT, TOP_LONG };

inline TopGesture updateTopButton(TopButton &button, bool down, uint32_t now, uint32_t longMs) {
  if (down && !button.wasDown) {
    button.downAt = now;
    button.longFired = false;
  }
  TopGesture gesture = TOP_NONE;
  if (down && !button.longFired && now - button.downAt >= longMs) {
    button.longFired = true;
    gesture = TOP_LONG;
  } else if (button.wasDown && !down && !button.longFired) {
    gesture = TOP_SHORT;
  }
  button.wasDown = down;
  return gesture;
}
