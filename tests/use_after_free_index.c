#include <stdlib.h>

void use_after_free_index() {
  int *ptr = malloc(sizeof(int) * 10);
  free(ptr);
  ptr[2] = 42;
}

int main() {
  use_after_free_index();
  return 0;
}
