#ifndef __X86_H__
#define __X86_H__

#include <stdint.h>

/* 从端口读取一字节数据 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t value = 0;
    asm volatile("inb %1, %0" : "=a"(value) : "d"(port));
    return value;
}

/* 写一字节数据到端口 */
static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" :: "a"(value), "d"(port));
}

/* 循环读取4字节端口数据到内存 */
static inline void insl(uint16_t port, void *dst, uint32_t cnt)
{
    asm volatile(
        "cld;"
        "repne; insl;"
        :"=D"(dst), "=c"(cnt)
        : "d"(port), "0"(dst), "1"(cnt)
        : "memory", "cc"
    );
}

#endif