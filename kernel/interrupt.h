/*
 *  kernel/interrupt.h
 *
 *  (C) 2021  Jacky
 */
#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "stdint.h"

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
/* 设置中断状态 */
void Idt_SetIntrStatus(IntrStatus status);
/* 注册中断处理函数 */
void Idt_RagisterHandler(uint8_t vecNr, intr_handler handler);

#endif