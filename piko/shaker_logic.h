#pragma once

#include <math.h>

struct ShakerParticle {
  float x;
  float y;
  float vx;
  float vy;
};

inline void shakerGravity(float ax, float ay, float uprightX, float uprightY,
                          float strength, float &gx, float &gy) {
  gx = (-ax * uprightY + ay * uprightX) * strength;
  gy = (ax * uprightX + ay * uprightY) * strength;
}

inline bool separateShakerParticles(ShakerParticle &a, ShakerParticle &b, float radius) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  float distanceSquared = dx * dx + dy * dy;
  float minimum = radius * 2;
  if (distanceSquared >= minimum * minimum) return false;
  float distance = sqrtf(distanceSquared);
  if (distance < 0.01f) {
    dx = 1;
    dy = 0;
    distance = 1;
  }
  float nx = dx / distance;
  float ny = dy / distance;
  float overlap = (minimum - distance) * 0.5f;
  a.x -= nx * overlap;
  a.y -= ny * overlap;
  b.x += nx * overlap;
  b.y += ny * overlap;
  float closingSpeed = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;
  if (closingSpeed < 0) {
    float impulse = -closingSpeed * 0.35f;
    a.vx -= impulse * nx;
    a.vy -= impulse * ny;
    b.vx += impulse * nx;
    b.vy += impulse * ny;
  }
  return true;
}

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
