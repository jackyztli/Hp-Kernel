/*
 *  kernel/global.h
 *
 *  (C) 2021  Jacky
 */
#ifndef GLOBAL_H
#define GLOBAL_H

#include "stdint.h"

/* 全局段描述符属性定义 */
#define DESC_G_4K      1
#define DESC_D_32      1
#define DESC_L         0             /* 64位标志，此处设为0表示32位 */
#define DESC_AVL       0             /* CPU预留给操作系统使用 */
#define DESC_P         1             /* 该段在内存中是否可用 */
#define T1_GDT         0             /* 全局描述符 */
#define T0_LDT         1             /* 局部描述符 */
#define RPL0           0             /* 特权级0 */
#define RPL1           1             /* 特权级1 */
#define RPL2           2             /* 特权级2 */
#define RPL3           3             /* 特权级3 */
#define DESC_S_CODE    1             /* 系统代码存储段 */
#define DESC_S_DATA    DESC_S_CODE   /* 系统数据存储段 */
#define DESC_S_SYS     0             /* 系统段，用于TSS和门描述符 */
#define DESC_TYPE_CODE 8             /* x=1,c=0,r=0,a=0 代码段可执行，不可读，已访问位a清0 */
#define DESC_TYPE_DATA 2             /* x=0,e=0,w=1,a=0 数据段不可执行，向上扩展，可写的，已访问位a清0*/

/* 全局段描述符选择子 */
/* 全局内核代码段描述符选择子 */
#define SELECTOR_K_CODE   ((1 << 3) + (T1_GDT << 2) + RPL0)
/* 全局内核数据段描述符选择子 */
#define SELECTOR_K_DATA   ((2 << 3) + (T1_GDT << 2) + RPL0)
/* 全局内核栈段描述符选择子 */
#define SELECTOR_K_STACK  SELECTOR_K_DATA
/* 全局内核显存描述符选择子 */
#define SELECTOR_K_GS     ((3 << 3) + (T1_GDT << 2) + RPL0)
/* 全局内核TSS描述符选择子 */
#define SELECTOR_K_TSS    ((4 << 3) + (T1_GDT << 2) + RPL0)
/* 全局用户代码段描述符选择子 */
#define SELECTOR_U_CODE   ((5 << 3) + (T1_GDT << 2) + RPL3)
/* 全局用户数据段描述符选择子 */
#define SELECTOR_U_DATA   ((6 << 3) + (T1_GDT << 2) + RPL3)
/* 全局用户栈段描述符选择子 */
#define SELECTOR_U_STACK  SELECTOR_U_DATA

#define GDT_ATTR_HIGH ((DESC_G_4K << 7) + \
                       (DESC_D_32 << 6) + \
                       (DESC_L << 5) + \
                       (DESC_AVL << 4))
/* 用户态全局代码段属性 */
#define GDT_U_CODE_ATTR_LOW ((DESC_P << 7) + \
                             (RPL3 << 5) + \
                             (DESC_S_CODE << 4) + \
                             DESC_TYPE_CODE)
/* 用户态全局数据段属性 */
#define GDT_U_DATA_ATTR_LOW ((DESC_P << 7) + \
                             (RPL3 << 5) + \
                             (DESC_S_DATA << 4) + \
                             DESC_TYPE_DATA)

/* 全局描述符起始地址 */
#define GDT_BASE_ADDR     0xc0000903
/* 全局描述符表项大小 */
#define GDT_ITEM_SIZE     0x8

#define EFLAGS_MBS	(1 << 1)	    /* 此项必须要设置 */
#define EFLAGS_IF_1	(1 << 9)	    /* if为1，开中断 */
#define EFLAGS_IF_0	0		        /* if为0，关中断 */
#define EFLAGS_IOPL_3	(3 << 12)	/* IOPL3，用于测试用户程序在非系统调用下进行IO */
#define EFLAGS_IOPL_0	(0 << 12)	/* IOPL0 */

/* 整除向上对齐 */
#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / STEP)

/* 结构体成员的偏移值 */
#define OFFSET(struct_name, member) (int32_t)(&((struct_name *)0)->member)

/* GDT描述符结构体 */
typedef struct {
    uint16_t limitLowWord;
    uint16_t baseLowWord;
    uint8_t  baseMidByte;
    uint8_t  attrLowByte;
    uint8_t  limitHighAttrHigh;
    uint8_t  baseHighByte;
} GDTDesc;

/* 创建GDT描述符 */
static inline GDTDesc MakeGDTDesc(uint32_t *descAddr, uint32_t limit, uint8_t attrLow, uint8_t attrHigh)
{
    uint32_t descBase = (uint32_t)descAddr;
    GDTDesc desc;
    desc.limitLowWord = limit & 0x0000ffff;
    desc.baseLowWord = descBase & 0x0000ffff;
    desc.baseMidByte = ((descBase) & 0x00ff0000) >> 16;
    desc.attrLowByte = attrLow;
    desc.limitHighAttrHigh = ((limit & 0x000f0000) >> 16) + (uint8_t)attrHigh;
    desc.baseHighByte = descBase >> 24;

    return desc;
}

/* 刷新GDTR寄存器 */
static inline void LoadGDTR(uint32_t size)
{
    uint64_t gdtOperand = (size * GDT_ITEM_SIZE - 1) | ((uint64_t)(uint32_t)GDT_BASE_ADDR << 16);
    __asm__ volatile ("lgdt %0" : : "m"(gdtOperand));
}

#endif