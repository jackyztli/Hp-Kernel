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

void memcpy(void *dst, void *src, size_t n)
{
    uint8_t *pdst = (uint8_t *)dst;
    uint8_t *psrc = (uint8_t *)src;
    while (n--) {
        *pdst = *psrc;
        pdst++;
        psrc++;
    }
}