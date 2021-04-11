/*
 *  kernel/tss.c
 *
 *  (C) 2021  Jacky
 */

#include "tss.h"
#include "stdint.h"
#include "kernel/global.h"
#include "kernel/thread.h"
#include "kernel/memory.h"
#include "kernel/console.h"
#include "lib/string.h"

/* 全局唯一TSS，所有进程共享这个TSS */
static TSS tss;

/* 更新tss中的esp0字段，用于特权级切换 */
void TSS_UpdateEsp(Task *task)
{
    tss.esp0 = (uint32_t *)((uintptr_t)task + PAGE_SIZE);
    return;
}

/* 初始化Tss */
void TSS_Init(void)
{
    Console_PutStr("TSS_Init start.\n");

    uint32_t tssSize = sizeof(tss);
    memset(&tss, 0, tssSize);
    tss.ss0 = SELECTOR_K_STACK;
    tss.ioBase = tssSize;

    /* TSS放在GDT中的第4个位置 */
    *((GDTDesc *)(GDT_BASE_ADDR + GDT_ITEM_SIZE * 4)) = MakeGDTDesc((uint32_t *)&tss, 
        tssSize - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);

    /* 用户态代码段放在GDT中第5个位置 */
    *((GDTDesc *)(GDT_BASE_ADDR + GDT_ITEM_SIZE * 5)) = MakeGDTDesc((uint32_t *)0, 
        0xfffff, GDT_U_CODE_ATTR_LOW, GDT_ATTR_HIGH);
    
    /* 用户态数据段放在GDT中第6个位置 */
    *((GDTDesc *)(GDT_BASE_ADDR + GDT_ITEM_SIZE * 6)) = MakeGDTDesc((uint32_t *)0, 
        0xfffff, GDT_U_DATA_ATTR_LOW, GDT_ATTR_HIGH);
    
    /* GDT的大小有变化，需要刷新全局描述符表 */
    LoadGDTR(7);
    /* 加载TR寄存器，正式使用进程 */
    __asm__ volatile ("ltr %w0" : : "r"(SELECTOR_K_TSS));

    Console_PutStr("TSS_Init end.\n");

    return;
}