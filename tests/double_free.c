#include <stdlib.h>

void double_free_bug() {
    int *ptr = malloc(sizeof(int) * 10);
    free(ptr);
    free(ptr); // Double free - should be detected
}

int main() {
    double_free_bug();
    return 0;
}
