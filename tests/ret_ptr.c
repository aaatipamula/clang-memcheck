#include <stdlib.h>

int *ret_ptr_unfreed() {
  int *ptr = malloc(sizeof(int) * 10);
  return ptr;
}

int *ret_ptr_freed() {
  int *ptr = malloc(sizeof(int) * 10);
  free(ptr);
  return ptr;
}

int main() {
  ret_ptr_unfreed();
  ret_ptr_freed();
}
