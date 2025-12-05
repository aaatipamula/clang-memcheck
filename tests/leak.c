#include <stdlib.h>

void memory_leak() {
    int *ptr = malloc(sizeof(int) * 10);
    // Missing free - should be detected
}

void no_leak() {
    int *ptr = malloc(sizeof(int) * 10);
    free(ptr);
}

int main() {
    memory_leak();
    no_leak();
    return 0;
}
