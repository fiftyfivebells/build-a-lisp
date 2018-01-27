#include <stdio.h>
#include <stdlib.h>

long power(long x, long y) {
    long result = x;
    for (int i = 0; i < y - 1; i++) {
        result *= x;
    }
    return result;
}

long min(long x, long y) {
    if (x < y) { return x; }
    else if (x >= y) { return y; }

    return 0;
}

long maximum(long x, long y) {
    if (x < y) { return y; }
    else if (x >= y) { return x; }

    return 0;
}