#include <assert.h>

#include "../piko/shaker_logic.h"

int main() {
  float gx, gy;
  shakerGravity(0.783f, -0.062f, 0.783f, -0.062f, 100, gx, gy);
  assert(fabsf(gx) < 0.01f);
  assert(gy > 60);

  ShakerParticle falling{50, 50, 0, 0};
  assert(!stepShakerParticle(falling, 0, 100, 0.1f, 0, 0, 100, 100, 3));
  assert(falling.y > 50);

  ShakerParticle wall{96, 50, 100, 0};
  assert(stepShakerParticle(wall, 0, 0, 0.1f, 0, 0, 100, 100, 3));
  assert(wall.x == 97);
  assert(wall.vx < 0);

  ShakerParticle a{50, 50, 10, 0};
  ShakerParticle b{53, 50, -10, 0};
  assert(separateShakerParticles(a, b, 3));
  assert(b.x - a.x >= 6);
  assert(a.vx < 10);
}
