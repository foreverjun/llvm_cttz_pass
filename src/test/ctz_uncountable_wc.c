#include <stdlib.h>
#include <stdint.h>

int count_trailing_zeroes(int32_t n) {
    int count = 0;

    if (n == 0){
        return 0;
    }
    while ((n & 1) == 0) {
        count += 1;
        n >>= 1;
    }

    return count;
}

int main(int argc, char **argv)
{
    int32_t n = atoi(argv[1]);
    int l = count_trailing_zeroes(n);
    return l;
}
