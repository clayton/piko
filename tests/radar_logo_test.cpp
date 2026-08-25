#include <assert.h>
#include <string.h>

#include "../radar/radar_logo.h"

int main() {
  char code[4];
  assert(radarLogoCode(code, "", "SKW4974") && !strcmp(code, "SKW"));
  assert(radarLogoCode(code, "DAL", "SKW6294") && !strcmp(code, "DAL"));
  assert(radarLogoCode(code, "", "ASI5965") && !strcmp(code, "ASI"));
  assert(radarLogoCode(code, "", "SCA69") && !strcmp(code, "SCA"));
  assert(!radarLogoCode(code, "", "N522RK") && !code[0]);
}
