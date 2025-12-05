#include <stdlib.h>

void try_reallocate_valid() {
  int *ptr = malloc(sizeof(int) * 10);
  int *tmp = realloc(ptr, sizeof(int) * 20);
  if (tmp == NULL) {
    // realloc failed — original ptr is still valid
    free(ptr);
    return;
  }
  free(tmp);
}

void try_rellocate_invalid() {
  int *ptr = malloc(sizeof(int) * 10);
  ptr = realloc(ptr, sizeof(int) * 20);
  free(ptr);
}

int main() {
  // try_reallocate_valid();
  try_rellocate_invalid();
}

