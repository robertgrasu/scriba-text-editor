#include <stdio.h>

int add(int a, int b) {
    int s = a + b;
    if (s > 10) {
        s = 10;
    }
    return s;
}

void main2(void) {
    printf("hi %d", add(1, 2));
}
