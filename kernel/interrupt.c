/*
 *  kernel/interrupt.c
 *
 *  (C) 2021  Jacky
 */

#include "interrupt.h"
#include "stdint.h"
#include "kernel/io.h"
#include "kernel/global.h"
#include "lib/print.h"

/* 当前支持的中断数 */
#define IDT_DESC_CNT 0x21

/* 8259A可编程中断控制器定义 */
#define PIC_M_CTRL 0x20             /* 主片控制端口为0x20 */
#define PIC_M_DATA 0x21             /* 主片数据端口为0x21 */
#define PIC_S_CTRL 0xa0             /* 从片控制端口为0xa0 */
#define PIC_S_DATA 0xa1             /* 从片数据端口为0xa1 */

#define EFLAGS_IF 0x00000200        /* eflags中的if位在第9位 */

/* 中断门描述符结构体 */
typedef struct {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t  dcount;
    uint8_t  attribute;
    uint16_t func_offset_high_word;
} IdtGateDesc;

/* 声明中断描述符表 */
static IdtGateDesc idt[IDT_DESC_CNT];

/* 中断处理函数入口表 */
extern intr_handler intr_entry_table[IDT_DESC_CNT];

/* 中断处理函数表 */
intr_handler idt_table[IDT_DESC_CNT];

/* 中断异常名 */
static char *intr_name[IDT_DESC_CNT];

/* 初始化中断描述符表 */
static void Idt_DescInit(void)
{
    put_str("Idt_DescInit start. \n");
    for (uint8_t i = 0; i < IDT_DESC_CNT; i++) {
        /* 中断处理入口函数低16位 */
        idt[i].func_offset_low_word = (uintptr_t)intr_entry_table[i] & 0x0000FFFF;
        /* 中断处理入口函数段描述符选择子 */
        idt[i].selector = SELECTOR_K_CODE;
        /* 未使用字段 */
        idt[i].dcount = 0;
        /* 中断描述符属性 */
        idt[i].attribute = IDT_DESC_ATTR_DPL0;
        /* 中断处理入口函数高16位 */
        idt[i].func_offset_high_word = ((uintptr_t)intr_entry_table[i] & 0xFFFF0000) >> 16;
    }

    put_str("Idt_DescInit end. \n");
    return;
}

/* 加载中断描述符寄存器 */
static inline void Idt_SetIdtRegister(void)
{
    /* 中断描述符寄存器低16位是表界限，高32位是表基址 */
    uint64_t idtOperand = (sizeof(idt) - 1) | ((uint64_t)(uintptr_t)idt << 16);
    /* lidt指令加载中断描述符寄存器 */
    __asm__ volatile("lidt %0" :: "m"(idtOperand));
}

/* 初始化可编程中断控制器8259A */
static void Idt_PciInit(void)
{
    put_str("Idt_PciInit start. \n");

    /* 初始化主片 */
    outb(PIC_M_CTRL, 0x11);     // ICW1: 边缘触发，级联8259，需要ICW4
    outb(PIC_M_DATA, 0x20);      // ICW2: 起始中断向量号为20，IR[0-7]为0x20 ~ 0x27

    outb(PIC_M_DATA, 0x04);      // ICW3：IR2接从片
    outb(PIC_M_DATA, 0x01);      // ICW4: 8086模式，正常EOI

    /* 初始化从片 */
    outb(PIC_S_CTRL, 0x11);      // ICW1: 边缘触发，级联8259，需要ICW4
    outb(PIC_S_DATA, 0x28);      // ICW2: 起始中断向量号为0x28

    outb(PIC_S_DATA, 0x02);      // ICW3: 设置从片链接主片的IR2管脚
    outb(PIC_S_DATA, 0x01);      // ICW4: 8086模式，正常EOI

    /* 只打开主片的IR0, 即只接受时钟中断 */
    outb(PIC_M_DATA, 0xfe);
    outb(PIC_S_DATA, 0xff);

    put_str("Idt_PciInit end. \n");
}

/* 通用中断处理函数，入参为中断号 */
static void Idt_GeneralIntrHendler(uint8_t vecNr)
{
    /* 0x27为伪中断，0x2f为系统保留，均无需处理 */
    if ((vecNr == 0x27) || (vecNr == 0x2f)) {
        return;
    }

    /* 发生异常，打印异常信息，系统挂起 */
    put_str("!!!!!!!!        excetion message begin        !!!!!!!!\n");
    put_str(intr_name[vecNr]);
    /* 缺页异常，将缺失的地址打印 */
    if (vecNr == 14) {
        int32_t pageFaultAddr = 0;
        /* 缺页异常的地址会存放到cr2寄存器 */
        __asm__ volatile("movl %%cr2, %0" : "=r"(pageFaultAddr));
        put_str("\npage fault addr is ");
        put_int(pageFaultAddr);
        put_str("\n");
    }

    put_str("!!!!!!!!        excetion message end        !!!!!!!!\n");

    /* 系统悬停，方便定位 */
    while (1) {

    }

    return;
}

/* 注册中断处理函数 */
void Idt_RagisterHandler(uint8_t vecNr, intr_handler handler)
{
    /* 中断处理函数用idt_table表记录 */
    idt_table[vecNr] = handler;
}

/* 异常处理函数表初始化 */
static void Idt_IdtTableInit(void)
{
    for (uint8_t i = 0; i < IDT_DESC_CNT; i++) {
        idt_table[i] = Idt_GeneralIntrHendler;
        intr_name[i] = "unknown";
    }

    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    /* intr_name[15] 第15项是intel保留项，未使用 */
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
    
    return;
}

/* 获取当前中断状态 */
IntrStatus Idt_GetIntrStatus(void)
{
    uint32_t eflags = 0;
    __asm__ volatile("pushfl; popl %0" : "=g"(eflags));
    return (eflags & EFLAGS_IF) ? INTR_ON : INTR_OFF;
}

/* 开中断 */
IntrStatus Idt_IntrEnable(void)
{
    if (Idt_GetIntrStatus() == INTR_ON) {
        return INTR_ON;
    }

    __asm__ volatile("sti");

    return INTR_OFF;
}

/* 关中断 */
IntrStatus Idt_IntrDisable(void)
{
    if (Idt_GetIntrStatus() == INTR_OFF) {
        return INTR_OFF;
    }

    __asm__ volatile("cli": : :"memory");

    return INTR_ON;
}

void Idt_SetIntrStatus(IntrStatus status)
{
    IntrStatus currentStatus = Idt_GetIntrStatus();
    if (currentStatus == status) {
        return;
    }

    if (status == INTR_ON) {
        Idt_IntrEnable();
    } else {
        Idt_IntrDisable();
    }

    return;
}

/* 中断初始化入口函数 */
void Idt_Init(void)
{
    put_str("Idt_Init start. \n");

    /* 初始化中断描述符表 */
    Idt_DescInit();

    /* 初始化可编程中断控制器8259A */
    Idt_PciInit();

    /* 中断处理函数表初始化 */
    Idt_IdtTableInit();

    /* 加载中断描述符寄存器 */
    Idt_SetIdtRegister();

    put_str("Idt_Init end. \n");

    return;
}