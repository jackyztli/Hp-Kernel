.section .text
handle_trap_entry:
    pushq   %rax
    pushq   %rbx
    pushq   %rcx
    pushq   %rdx
    pushq   %rbp
    pushq   %rdi
    pushq   %rsi
    pushq   %r8
    pushq   %r9
    pushq   %r10
    pushq   %r11
    pushq   %r12
    pushq   %r13
    pushq   %r14
    pushq   %r15

    movq    %es,    %rax
    pushq   %rax
    movq    %ds,    %rax
    pushq   %rax

    cld
    # 此时栈信息如下：
    # SS            0xb8
    # ESP           0xb0
    # EFLAGS        0xa8
    # CS            0xa0
    # EIP           0x98
    # Error Code    0x90
    # Func          0x88
    # RAX           0x80
    # RBX           0x78
    # RCX           0x70
    # RDX           0x68
    # RBP           0x60
    # RDI           0x58
    # RSI           0x50
    # R8            0x48
    # R9            0x40
    # R10           0x38
    # R11           0x30
    # R12           0x28
    # R13           0x20
    # R14           0x18
    # R15           0x10
    # ES            0x08
    # DS            0x00

    # 获取错误码，Error Code，作为Func的第二个参数
    movq    0x90(%rsp), %rsi
    movq    0x88(%rsp), %rdx

    xorq    %rax,   %rax
    movq    $0x10,  %rax
    movq    %rax,   %ds
    movq    %rax,   %es

    # Func的第一个参数
    movq    %rsp,   %rdi 
    callq   *%rdx

.globl ret_from_exception
ret_from_exception:
    popq    %rax
    movq    %rax,   %ds
    popq    %rax
    movq    %rax,   %es
    popq    %r15
    popq    %r14
    popq    %r13
    popq    %r12
    popq    %r11
    popq    %r10
    popq    %r9
    popq    %r8
    popq    %rsi
    popq    %rdi
    popq    %rbp
    popq    %rdx
    popq    %rcx
    popq    %rbx
    popq    %rax
    # 跳过Func和Error Code
    addq    $0x10,  %rsp
    iretq

.globl divide_error
divide_error:
    # 由于divide_error不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_divide_error(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向do_divide_error的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl debug
debug:
    # 由于debug不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_debug(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向debug的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl nmi
nmi:
    # 由于nmi不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq %rax
    leaq do_nmi(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向nmi的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl int3
int3:
    # 由于nmi不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_int3(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向int3的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl overflow
overflow:
    # 由于overflow不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_overflow(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向overflow的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl bounds
bounds:
    # 由于bounds不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_bounds(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向bounds的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl invalid_op
invalid_op:
    # 由于invalid_op不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_invalid_op(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向invalid_op的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl device_not_available
device_not_available:
    # 由于device_not_available不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_device_not_available(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向device_not_available的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl double_fault
double_fault:
    # 由于double_fault不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq %rax
    leaq do_double_fault(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向double_fault的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl coprocessor_segment_overrun
coprocessor_segment_overrun:
    # 由于coprocessor_segment_overrun不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_coprocessor_segment_overrun(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向coprocessor_segment_overrun的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl invalid_TSS
invalid_TSS:
    # 由于invalid_TSS不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_invalid_TSS(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向invalid_TSS的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl segment_not_present
segment_not_present:
    # 由于segment_not_present不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_segment_not_present(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向segment_not_present的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl stack_segment_fault
stack_segment_fault:
    # 由于stack_segment_fault不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_stack_segment_fault(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向stack_segment_fault的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl general_protection
general_protection:
    # 由于general_protection不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_general_protection(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向general_protection的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl page_fault
page_fault:
    # 由于page_fault不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_page_fault(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向page_fault的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl reserved
reserved:
    # 由于reserved不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_reserved(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向reserved的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry

.globl coprocessor_error
coprocessor_error:
    # 由于coprocessor_error不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_coprocessor_error(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向coprocessor_error的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
 
.globl alignment_check
alignment_check:
    # 由于alignment_check不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_alignment_check(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向alignment_check的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl machine_check
machine_check:
    # 由于machine_check不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_machine_check(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向machine_check的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl SIMD_exception
SIMD_exception:
    # 由于SIMD_exception不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_SIMD_exception(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向SIMD_exception的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
    
.globl virtualization_exception
virtualization_exception:
    # 由于virtualization_exception不产生Error Code，这里压入0充当栈信息中的Error Code
    pushq $0
    pushq %rax
    leaq do_virtualization_exception(%rip), %rax
    # 这里将栈顶和rax交换，此时rax是指向virtualization_exception的地址，所以这里是将
    # 函数地址放在栈顶上，充当栈信息中的Func
    xchgq %rax, (%rsp)
    jmp handle_trap_entry
