#include <assert.h>
#include <stdint.h>

#include "../piko/button_logic.h"

int main() {
  TopButton button{};
  assert(updateTopButton(button, true, 100, 900) == TOP_NONE);
  assert(updateTopButton(button, false, 300, 900) == TOP_SHORT);
  assert(updateTopButton(button, true, 1000, 900) == TOP_NONE);
  assert(updateTopButton(button, true, 1900, 900) == TOP_LONG);
  assert(updateTopButton(button, false, 2000, 900) == TOP_NONE);
}
