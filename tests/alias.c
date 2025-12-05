#include <stdlib.h>

void aliasing() {
  int *ptr1 = malloc(sizeof(int) * 10);
  int *ptr2;
  ptr2 = ptr1;
  free(ptr2);
}

int main() {
  aliasing();
  return 0;
}
