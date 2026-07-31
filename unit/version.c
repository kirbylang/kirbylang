#include <assert.h>
#include <string.h>

#include "../src/version.h"

int main(void) {
  assert(strcmp(KIRBY_VERSION, "0.2.0") == 0);
  return 0;
}
