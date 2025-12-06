#include <stdlib.h>

void invalid_free(int *ptr) {
  free(ptr);
}

int main() {
  int * thing = malloc(sizeof(int));
  invalid_free(thing);
  *thing = 42;
}
