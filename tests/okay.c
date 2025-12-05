#include <stdlib.h>

void properly_managed() {
  int *ptr = malloc(sizeof(int) * 10);

  for (int i = 0; i < 10; i++) {
    ptr[i] = 2*i;
  }

  free(ptr);
}

int main() {

}
