#pragma once

#include <math.h>

struct ShakerParticle {
  float x;
  float y;
  float vx;
  float vy;
};

inline bool stepShakerParticle(ShakerParticle &particle, float gx, float gy, float dt,
                               float left, float top, float right, float bottom, float radius) {
  particle.vx = (particle.vx + gx * dt) * 0.995f;
  particle.vy = (particle.vy + gy * dt) * 0.995f;
  particle.x += particle.vx * dt;
  particle.y += particle.vy * dt;

  bool hit = false;
  if (particle.x < left + radius) {
    particle.x = left + radius;
    particle.vx = fabsf(particle.vx) * 0.4f;
    hit = true;
  } else if (particle.x > right - radius) {
    particle.x = right - radius;
    particle.vx = -fabsf(particle.vx) * 0.4f;
    hit = true;
  }
  if (particle.y < top + radius) {
    particle.y = top + radius;
    particle.vy = fabsf(particle.vy) * 0.4f;
    hit = true;
  } else if (particle.y > bottom - radius) {
    particle.y = bottom - radius;
    particle.vy = -fabsf(particle.vy) * 0.4f;
    hit = true;
  }
  if (hit) {
    particle.vx *= 0.9f;
    particle.vy *= 0.9f;
    if (fabsf(particle.vx) < 8) particle.vx = 0;
    if (fabsf(particle.vy) < 8) particle.vy = 0;
  }
  return hit;
}
