#ifndef __TRAP_H__
#define __TRAP_H__

#include <stdint.h>

#define MAX_IDT_NUM 256

/* 开启中断 */
static inline void sti(void)
{
    __asm__ __volatile__ ("sti;hlt":::"memory");
}

void init_trap(void);

#endif