.section .text
.globl handle_trap_entry
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
    movq    $0x10,  %rax
    movq    %rax,   %ds
    movq    %rax,   %es

    leaq trap_msg(%rip), %rax
    pushq %rax
    movq %rax, %rdi
    movq $0, %rax
    callq puts
    addq $0x8, %rsp

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
    iretq

.section .data
trap_msg:
    .asciz "Unknown interrupt or fault at RIP\n"
