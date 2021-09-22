/* 系统异常处理
*/

#include <trap.h>
#include <stdint.h>
#include <system.h>
#include <printk.h>

void divide_error(void);
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment_fault(void);
void general_protection(void);
void page_fault(void);
void reserved(void);
void coprocessor_error(void);
void alignment_check(void);
void machine_check(void);
void SIMD_exception(void);
void virtualization_exception(void);
void system_call(void);

void do_divide_error(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_divide_error(0), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {

    }
}

void do_debug(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_debug(1), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_nmi(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_nmi(2), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_int3(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_int3(3), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_overflow(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_overflow(4), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_bounds(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_bounds(5), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_invalid_op(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_invalid_op(6), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_device_not_available(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_device_not_available(7), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_double_fault(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_double_fault(8), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_coprocessor_segment_overrun(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_coprocessor_segment_overrun(9), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_invalid_TSS(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_invalid_TSS(10), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_segment_not_present(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_segment_not_present(11), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_stack_segment_fault(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_stack_segment_fault(12), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_general_protection(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_general_protection(13), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_page_fault(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_page_fault(14), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_reserved(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_reserved(15), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_coprocessor_error(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_coprocessor_error(16), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_alignment_check(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_alignment_check(17), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_machine_check(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_machine_check(18), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_SIMD_exception(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_SIMD_exception(19), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void do_virtualization_exception(uintptr_t rsp, uint64_t error_code)
{
	uint64_t *p = (uint64_t *)(rsp + 0x98);
	printk("do_virtualization_exception(20), ERROR_CODE: %ld, RSP: %ld, RIP: %ld\n", error_code, rsp, *p);
	while(1) {
        
    }
}

void init_trap(void)
{
    set_trap_gate(0, (uintptr_t)divide_error);
	set_trap_gate(1, (uintptr_t)debug);
	set_trap_gate(2, (uintptr_t)nmi);
	set_system_gate(3, (uintptr_t)int3);
	set_system_gate(4, (uintptr_t)overflow);
	set_system_gate(5, (uintptr_t)bounds);
	set_trap_gate(6, (uintptr_t)invalid_op);
	set_trap_gate(7, (uintptr_t)device_not_available);
	set_trap_gate(8, (uintptr_t)double_fault);
	set_trap_gate(9, (uintptr_t)coprocessor_segment_overrun);
	set_trap_gate(10, (uintptr_t)invalid_TSS);
	set_trap_gate(11, (uintptr_t)segment_not_present);
	set_trap_gate(12, (uintptr_t)stack_segment_fault);
	set_trap_gate(13, (uintptr_t)general_protection);
	set_trap_gate(14, (uintptr_t)page_fault);
	set_trap_gate(15, (uintptr_t)reserved);
	set_trap_gate(16, (uintptr_t)coprocessor_error);
	set_trap_gate(17, (uintptr_t)alignment_check);
	set_trap_gate(18, (uintptr_t)machine_check);
	set_trap_gate(19, (uintptr_t)SIMD_exception);
	set_trap_gate(20, (uintptr_t)virtualization_exception);

	/* 系统调用 */
	set_system_gate(0x80, (uintptr_t)system_call);
}
