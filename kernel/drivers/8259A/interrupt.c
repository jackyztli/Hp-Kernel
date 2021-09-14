#include <interrupt.h>
#include <printk.h>
#include <io.h>
#include <trap.h>
#include <system.h>

#define SAVE_ALL_ARGS                \
    "pushq   %rax;             \n\t" \
    "pushq   %rbx;             \n\t" \
    "pushq   %rcx;             \n\t" \
    "pushq   %rdx;             \n\t" \
    "pushq   %rbp;             \n\t" \
    "pushq   %rdi;             \n\t" \
    "pushq   %rsi;             \n\t" \
    "pushq   %r8;              \n\t" \
    "pushq   %r9;              \n\t" \
    "pushq   %r10;             \n\t" \
    "pushq   %r11;             \n\t" \
    "pushq   %r12;             \n\t" \
    "pushq   %r13;             \n\t" \
    "pushq   %r14;             \n\t" \
    "pushq   %r15;             \n\t" \
    "movq    %es,    %rax;     \n\t" \
    "pushq   %rax;             \n\t" \
    "movq    %ds,    %rax;     \n\t" \
    "pushq   %rax;             \n\t" \
    "xorq    %rax,   %rax;     \n\t" \
    "movq    $0x10,  %rax;     \n\t" \
    "movq    %rax,   %ds;      \n\t" \
    "movq    %rax,   %es;      \n\t"

void ret_from_exception(void);

void do_IRQ(uintptr_t rsp, uint64_t nr)
{
	printk("do_IRQ: %ld\n", nr);
	uint8_t x = inb(0x60);
	printk("key code: %c\n", x);
	outb(0x20,0x20);
}

void do_timer(uintptr_t rsp, uintptr_t nr)
{
    __asm__ volatile(
        "pushq $0;              \n\t" \
        SAVE_ALL_ARGS                \
        "movq %rsp, %rdi;       \n\t" \
        "leaq ret_from_exception(%rip), %rax; \n\t" \
        "pushq %rax;             \n\t" \
        "movq $0x20, %rsi;       \n\t" \
        "jmp  do_IRQ;            \n\t"
    );
}

void do_keyboard(uintptr_t rsp, uintptr_t nr)
{
    __asm__ volatile(
        "pushq $0;              \n\t" \
        SAVE_ALL_ARGS                 \
        "movq %rsp, %rdi;       \n\t" \
        "leaq ret_from_exception(%rip), %rax; \n\t" \
        "pushq %rax;            \n\t" \
        "movq $0x21, %rsi;      \n\t" \
        "jmp  do_IRQ;           \n\t"
    );
}

void init_interrupt(void)
{
    printk("8259A init...\n");

    set_intr_gate(0x20, (uintptr_t)do_timer);
    set_intr_gate(0x21, (uintptr_t)do_keyboard);

    outb(0x20, 0x11);
    outb(0x21, 0x20);
    outb(0x21, 0x04);
    outb(0x21, 0x01);

    outb(0xa0, 0x11);
    outb(0xa1, 0x28);
    outb(0xa1, 0x02);
    outb(0xa1, 0x01);

    outb(0x21, 0xfd);
    outb(0xa1, 0xff);
    
    /* 开启中断 */
    sti();
}