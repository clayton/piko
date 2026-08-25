#include <assert.h>

#include "../piko/raises_logic.h"

int main() {
  assert(selectedIncident(true, 4, 6) == 0);
  assert(selectedIncident(false, 4, 6) == 4);
  assert(selectedIncident(false, 4, 2) == 0);
  assert(feedDismissed(20, 20));
  assert(!feedDismissed(21, 20));
  assert(!feedDismissed(0, 0));
}
