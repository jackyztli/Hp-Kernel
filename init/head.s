# 这是一个head程序。
# 其位于kernel的开头处。
# loader程序将其读取到0x100000处，由setup程序
.code64
.globl setup64
setup64:
    # 64位初始化
    cli

    # 加载64位的全局描述符表
    lgdt gdt64desc(%rip)
    lidt idt64desc(%rip)

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    # 临时栈，用于跳转到long模式使用
    movq $0x7e00, %rsp

    # 加载内核页目录基址
    movq $0x101000, %rax
    movq %rax, %cr3
    movq switch_seg(%rip), %rax
    pushq $0x8
    pushq %rax
    lretq

switch_seg:
    .quad entry64

entry64:
    movq    $0x10,  %rax
    movq    %rax,   %ds
    movq    %rax,   %es
    movq    %rax,   %gs
    movq    %rax,   %ss
    movq $0xffff800000007e00, %rsp

    # 进入到64位长模式
    leaq kernel_init(%rip), %rax
    pushq $0x8
    pushq %rax
    lretq

    hlt

# 内核页目录基址从0x100000 + 0x1000开始
.align 8
.org 0x1000
__PML4E:
    .quad 0x102007
    .fill 255, 8, 0
    .quad 0x102007
    .fill 255, 8, 0

.org 0x2000
__PDPTE:
    .quad 0x103003
    .fill 511, 8, 0

# 一个PDT项对应2M的物理内存空间
.org 0x3000
__PDT:
    # 前10M映射
    .quad   0x000083
    .quad   0x200083
    .quad   0x400083
    .quad   0x600083
    .quad   0x800083
    # 显存物理地址，映射到0xa00000的线性地址处
    .quad	0xe0000083
    .quad	0xe0200083
    .quad	0xe0400083
    .quad	0xe0600083
    .quad	0xe0800083
    .quad	0xe0a00083
    .quad	0xe0c00083
    .quad	0xe0e00083
    .fill	499,8,0

.p2align 2
# idt64定义
.globl idt64
idt64:
    .fill 512, 8, 0

# gdt64定义
.globl gdt64
gdt64:
    .word 0, 0, 0, 0                # 空段
    .quad 0x00209b0000000000        # 内核态代码段
    .quad 0x0000930000000000        # 内核态数据段
    .quad 0x0020f80000000000        # 用户态代码段
    .quad 0x0000f20000000000        # 用户态数据段
    .quad 0x00cf9a000000ffff        # 32位内核态代码段
    .quad 0x00cf92000000ffff        # 32位内核态数据段
    .fill 10, 8, 0
gdt64_end:

.globl idt64desc
idt64desc:
    .word 4096 - 1
    .quad idt64

gdt64desc:
    .word gdt64_end - gdt64 - 1     # 段界限
    .quad gdt64                     # 段基址
