/* 系统异常处理
*/

#include <trap.h>
#include <stdint.h>

extern void handle_trap_entry(void);

void idt_desc_init(void)
{
    /* 在64位系统上，段描述符占8个字节 */
    uintptr_t trap_func = (uintptr_t)handle_trap_entry;
    uint16_t func_offset_low_word = trap_func & (uintptr_t)0xffff;
    uint16_t selector = 0x8;
    uint16_t attribute = 0x8E00;
    uint16_t func_offset_mid_word = (trap_func >> 16) & (uintptr_t)0xffff;
    uint32_t func_offset_high_quad = (trap_func >> 32) & (uintptr_t)0xffffffff;

    for (uint32_t i = 0; i < MAX_IDT_NUM; i++) {
        idt64[i].func_offset_low_word = func_offset_low_word;
        idt64[i].selector = selector;
        idt64[i].attribute = attribute;
        idt64[i].func_offset_mid_word = func_offset_mid_word;
        idt64[i].func_offset_high_quad = func_offset_high_quad;
    }
}

void trap_init(void)
{
    /* 初始化idt段描述符 */
    idt_desc_init();
}
