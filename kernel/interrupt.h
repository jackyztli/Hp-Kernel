/*
 *  kernel/interrupt.h
 *
 *  (C) 2021  Jacky
 */
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "stdint.h"

#define RPL0 0
#define RPL1 1
#define RPL2 2
#define RPL3 3

#define T1_GDT 0
#define T0_LDT 1

/* 中断处理函数段选择子 */
#define SELECTOR_K_CODE ((1 << 3) + (T1_GDT << 2) + RPL0)
#define SELECTOR_K_DATA ((2 << 3) + (T1_GDT << 2) + RPL0)

/* 中断描述符属性 */
#define IDT_DESC_P 1
#define IDT_DESC_DPL0 0
#define IDT_DECS_DPL3 3
#define IDT_DESC_32_TYPE 0xE
#define IDT_DESC_ATTR_DPL0 \
    ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)
#define IDT_DESC_ATTR_DPL3 \
    ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)

/* 中断处理函数类型定义 */
typedef void * intr_handler;

/* 中断初始化入口函数 */
void Idt_Init(void);

/* 中断状态相关定义 */
typedef enum {
    INTR_OFF,             /* 当前为关中断状态 */
    INTR_ON               /* 当前为开中断状态 */
} IntrStatus;

/* 获取当前中断状态 */
IntrStatus Idt_GetIntrStatus(void);
/* 开中断 */
IntrStatus Idt_IntrEnable(void);
/* 关中断 */
IntrStatus Idt_IntrDisable(void);

#endif