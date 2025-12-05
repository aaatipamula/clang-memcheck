#include <stdlib.h>

void overwrite_mem() {
  int *ptr = malloc(sizeof(int) * 20);
  // previously malloced memory is lost
  ptr = malloc(sizeof(int) * 400);
  free(ptr);
}

int main () {
  overwrite_mem();
}
