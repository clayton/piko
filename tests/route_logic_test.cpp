#include <assert.h>
#include <string.h>

#include "../radar/route_logic.h"

int main() {
  assert(!routeNearReceiver(33.6229f, -111.9105f, 39.8719f, -75.2411f, 28.4294f, -81.3090f));
  assert(routeNearReceiver(33.6229f, -111.9105f, 33.4342f, -112.0116f, 38.7487f, -90.3700f));
  assert(!strcmp(displayAircraftType("B38M"), "B737M8"));
  assert(!strcmp(displayAircraftType("A320"), "A320"));
}
