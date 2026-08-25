#include <assert.h>
#include <stdint.h>

#include "../piko/power_logic.h"

int main() {
  assert(!shouldIdle(false, true, 44999, 0, 45000));
  assert(shouldIdle(false, true, 45000, 0, 45000));
  assert(!shouldIdle(true, true, 90000, 0, 45000));
  assert(!shouldIdle(false, false, 90000, 0, 45000));

  assert(!motionDetected(0.03f, 0.03f, 0.03f, 0, 0, 0, 0.12f));
  assert(motionDetected(0.13f, 0, 0, 0, 0, 0, 0.12f));
}
