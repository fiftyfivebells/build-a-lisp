#include <stdio.h>
#include <stdlib.h>

long power(long x, long y) {
    long result = x;
    for (int i = 0; i < y - 1; i++) {
        result *= x;
    }
    return result;
}