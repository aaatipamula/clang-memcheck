#include <stdlib.h>

void use_after_free_deref() {
  int *ptr = malloc(sizeof(int) * 10);
  free(ptr);
  *ptr = 42;
}

int main() {
  use_after_free_deref();
  return 0;
}
