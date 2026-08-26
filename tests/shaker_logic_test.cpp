#include <assert.h>

#include "../piko/shaker_logic.h"

int main() {
  ShakerParticle falling{50, 50, 0, 0};
  assert(!stepShakerParticle(falling, 0, 100, 0.1f, 0, 0, 100, 100, 3));
  assert(falling.y > 50);

  ShakerParticle wall{96, 50, 100, 0};
  assert(stepShakerParticle(wall, 0, 0, 0.1f, 0, 0, 100, 100, 3));
  assert(wall.x == 97);
  assert(wall.vx < 0);
}
