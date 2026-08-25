#include <assert.h>
#include <stdint.h>

#include "../piko/power_logic.h"

int main() {
  assert(!shouldIdle(false, true, false, 44999, 0, 45000));
  assert(shouldIdle(false, true, false, 45000, 0, 45000));
  assert(!shouldIdle(false, true, true, 90000, 0, 45000));
  assert(!shouldIdle(false, true, false, 5000, 5001, 45000));
  assert(!shouldIdle(true, true, false, 90000, 0, 45000));
  assert(!shouldIdle(false, false, false, 90000, 0, 45000));

  assert(!motionDetected(0.03f, 0.03f, 0.03f, 0, 0, 0, 0.12f));
  assert(motionDetected(0.13f, 0, 0, 0, 0, 0, 0.12f));
}
