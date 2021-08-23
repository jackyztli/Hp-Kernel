#ifndef __PRINTF_H__
#define __PRINTF_H__

#include <stdint.h>

#define MAX_PRINTK_LEN 4096

/* 内核打印调试信息到串口 */
void printk(const char *fmt, ...);

#endif