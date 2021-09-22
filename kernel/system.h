#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdint.h>

struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint16_t attribute;
    uint16_t func_offset_mid_word;
    uint32_t func_offset_high_quad;
    uint32_t reserve;
} __attribute__((packed));

#define MAX_GDT_NUM 12
#define MAX_IDT_NUM 256
/* GDT描述符 */
extern struct gate_desc gdt64[MAX_GDT_NUM];
/* IDT描述符 */
extern struct gate_desc idt64[MAX_IDT_NUM];

static inline void make_idt_desc(uint8_t n, uint16_t attr, uint16_t selector, uintptr_t trap_func)
{
    /* 在64位系统上，段描述符占16个字节，其结构如下：
     *  127                                         96                                          64
     *  -----------------------------------------------------------------------------------------
     *  |                   预留                     |               处理函数高32位               |
     *  -----------------------------------------------------------------------------------------
     *  63                   48 47 44 43   40        31                   15                     0
     *  -----------------------------------------------------------------------------------------
     *  | 处理函数低32位的高16位|P|DPL|0|TYPE|    0    |       段选择子      |处理函数低32位的低16位|
     *  -----------------------------------------------------------------------------------------
     *  属性值说明：
     *      P：表示是否在位
     *      DPL：特权级，x86中内核是0特权级，用户态是3特权级
     *      TYPE：段描述符标志，E表示中断门，F表示陷阱门
     */
    idt64[n].func_offset_low_word = trap_func & (uintptr_t)0xffff;
    idt64[n].selector = selector;
    idt64[n].attribute = attr;
    idt64[n].func_offset_mid_word = (trap_func >> 16) & (uintptr_t)0xffff;
    idt64[n].func_offset_high_quad = (trap_func >> 32) & (uintptr_t)0xffffffff;
}

/* 中断门描述符 */
static inline void set_intr_gate(uint32_t n, uintptr_t trap_func)
{
    /* P=1、DPL=0、TYPE=E */
    make_idt_desc(n, 0x8E00, 0x8, trap_func);
}

/* 陷阱门描述符，DPL=0，用于异常处理 */
static inline void set_trap_gate(uint32_t n, uintptr_t trap_func)
{
    /* P=1、DPL=0、TYPE=F */
    make_idt_desc(n, 0x8F00, 0x8, trap_func);
}

/* 陷阱门描述符，DPL=3，用于系统调用 */
static inline void set_system_gate(uint32_t n, uintptr_t trap_func)
{
    /* P=1、DPL=3、TYPE=F */
    make_idt_desc(n, 0xEF00, 0x8, trap_func);
}

/* 加载TSS选择子 */
static inline void load_tr(uint16_t n)
{
    __asm__ volatile("ltr %%ax" : :"a"(n << 3) :"memory");
}

#endif