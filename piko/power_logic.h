#pragma once

#include <math.h>
#include <stdint.h>

inline bool shouldIdle(bool idle, bool inApp, bool pluggedIn,
                       uint32_t now, uint32_t lastActivity, uint32_t timeout) {
  return !idle && inApp && !pluggedIn &&
         static_cast<int32_t>(now - lastActivity) >= static_cast<int32_t>(timeout);
}

inline bool motionDetected(float x, float y, float z,
                           float lastX, float lastY, float lastZ, float threshold) {
  return fabsf(x - lastX) + fabsf(y - lastY) + fabsf(z - lastZ) > threshold;
}
