/* 内核调试信息打印
 */

#include <printk.h>
#include <stdarg.h>
#include <stdint.h>
#include <console.h>

static char *itoa(char *str, int64_t num)
{
    /* 负数 */
    if (num < 0) {
        *str = '-';
        str++;
        num = -num;
    }

    int64_t exp = 1;
    int64_t numTmp = num / 10;
    while (numTmp) {
        exp *= 10;
        numTmp /= 10;
    }

    numTmp = num;
    while (exp) {
        *str = numTmp / exp + '0';
        str++;
        numTmp %= exp;
        exp /= 10;
    }

    /* 返回转换后的字符串指针 */
    return str;
}

int32_t vsprintf(char *buf, const char *fmt, va_list args)
{
    char *str = buf;
    const char *fmtTmp = fmt;
    for (; *fmtTmp; fmtTmp++) {
        if (*fmtTmp != '%') {
            *str = *fmtTmp;
            str++;
            continue;
        }

        fmtTmp++;
        switch (*fmtTmp) {
            case 'c':
                *str = (uint8_t)va_arg(args, int32_t);
                str++;
                break;
            
            case 's': {
                char *s = va_arg(args, char *);
                while (*s != '\0') {
                    *str = *s;
                    str++;
                    s++;
                }

                break;
            }

            case 'd': {
                int32_t num = va_arg(args, int32_t);
                str = itoa(str, num);
                break;
            }

            case 'l': {
                fmtTmp++;
                if (*fmtTmp == 'd') {
                    int64_t num = va_arg(args, int64_t);
                    str = itoa(str, num);
                }
                break;
            }
        }
    }

    *str = '\0';
    return str - buf;
}

void printk(const char *fmt, ...)
{
    char buf[MAX_PRINTK_LEN] = {0};
    va_list args;
    va_start(args, fmt);
    (void)vsprintf(buf, fmt, args);
    va_end(args);

    puts(buf);
}