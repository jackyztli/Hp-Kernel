/*
 *  kernel/tss.h
 *
 *  (C) 2021  Jacky
 */
#ifndef TSS_H
#define TSS_H

#include "stdint.h"
#include "kernel/thread.h"
#include "kernel/global.h"

typedef struct {
    uint32_t lastTaskTss;    /* 上一个任务的TSS指针 */
    uint32_t *esp0;          /* 特权级0的esp寄存器 */
    uint32_t ss0;            /* 特权级0的栈段选择子 */
    uint32_t *esp1;          /* 特权级1的esp寄存器，未被使用 */
    uint32_t ss1;            /* 特权级1的栈段选择子，未被使用 */
    uint32_t *esp2;          /* 特权级2的esp寄存器，未被使用 */
    uint32_t ss2;            /* 特权级2的栈段选择子，未被使用 */
    uint32_t cr3;            /* cr3寄存器 */
    uint32_t (*eip)(void);   /* 进程运行函数 */
    uint32_t eflags;         /* 进程eflags寄存器 */
    uint32_t eax;            /* 8个普通寄存器 */
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;             /* 段寄存器 */
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;            /* 本地描述符选择子，未被使用 */
    uint32_t trace;
    uint32_t ioBase;         /* I/O位图在TSS中的偏移地址 */
} TSS;

/* TSS段属性定义 */
#define DESC_TYPE_TSS  9             /* B位设置为0，表示TSS不忙 */
#define DESC_D_TSS     0

#define TSS_ATTR_HIGH  ((DESC_G_4K << 7) + \
                        (DESC_D_TSS << 6) + \
                        (DESC_L << 5) + \
                        (DESC_AVL << 4) + 0x0)

#define TSS_ATTR_LOW   ((DESC_P << 7) + \
                        (RPL0 << 5) + \
                        (DESC_S_SYS << 4) + \
                        DESC_TYPE_TSS)

/* 更新tss中的esp0字段，用于特权级切换 */
void TSS_UpdateEsp(Task *task);
/* 初始化Tss */
void TSS_Init(void);

#endif