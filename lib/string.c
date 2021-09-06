#include <string.h>
#include <stdint.h>

void memset(void *s, int8_t c, size_t n)
{
    int8_t *ps = (int8_t *)s;
    while (n--) {
        *ps = c;
        ps++;
    }
}