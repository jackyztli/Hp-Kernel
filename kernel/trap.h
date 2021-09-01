#ifndef __TRAP_H__
#define __TRAP_H__

#include <stdint.h>

struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint16_t attribute;
    uint16_t func_offset_mid_word;
    uint32_t func_offset_high_quad;
    uint32_t reserve;
};

#define MAX_IDT_NUM 256

extern struct gate_desc idt64[MAX_IDT_NUM];

/* 开启中断 */
static inline void sti()
{
    __asm__ __volatile__ ("sti;hlt":::"memory");
}

void trap_init(void);

#endif