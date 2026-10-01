#include "../src/vm.h"

// Linked against kirby_runtime only, without the parser, type checker or
// compiler. Fails to link if the runtime depends on any of them.
int main(void) {
  initVM(0, NULL);
  freeVM();
  return 0;
}
