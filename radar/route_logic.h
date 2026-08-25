#pragma once

#include <math.h>
#include <string.h>

inline bool routeNearReceiver(float receiverLat, float receiverLon,
                              float originLat, float originLon,
                              float destinationLat, float destinationLon,
                              float maxDistanceNm = 200.0f) {
  const float lonScale = 60.0f * cosf(receiverLat * 0.01745329252f);
  const float ox = (originLon - receiverLon) * lonScale;
  const float oy = (originLat - receiverLat) * 60.0f;
  const float dx = (destinationLon - originLon) * lonScale;
  const float dy = (destinationLat - originLat) * 60.0f;
  const float lengthSquared = dx * dx + dy * dy;
  const float position = lengthSquared ? fminf(1.0f, fmaxf(0.0f, -(ox * dx + oy * dy) / lengthSquared)) : 0.0f;
  return hypotf(ox + position * dx, oy + position * dy) <= maxDistanceNm;
}

inline const char *displayAircraftType(const char *icaoType) {
  return icaoType && !strcmp(icaoType, "B38M") ? "B737M8" : icaoType;
}
