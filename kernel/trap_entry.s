.section .text
.globl handle_trap_entry
handle_trap_entry:
#    pushad
    movq %es, %rax
    pushq %rax

    movq %ds, %rax
    pushq %rax

    movq $0x10, %rax
    movq %rax, %ds
    movq %rax, %es

    leaq trap_msg(%rip), %rdi
    pushq %rdi
    callq printk
    addq $0x8, %rsp

.section .data
trap_msg:
    .asciz "Unknown interrupt or fault at RIP\n"
