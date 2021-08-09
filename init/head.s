# 这是一个head程序。
# 其位于kernel的开头处。
# boot程序将其读取到0x100000处，并执行
.code64
.globl setup
setup:
    # 注意，此处还是运行在32位的段中
    cli

    # 载入全局段描述符
    lgdt gdt64desc

    # 当前还是运行在32位的内核态
    movw $0x30, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    # 栈从0x80000开始，页表从0x90000开始
    mov $0x80000, %ebp
    mov %ebp, %esp

    # 设置初步页表，后续进入long模式后，需要重新部署页表
    movq $0x91007, (0x90000)
    movq $0x91007, (0x90800)
    movq $0x92007, (0x91000)
    movq $0x000083, (0x92000)
    movq $0x200083, (0x92008)
    movq $0x400083, (0x92010)
    movq $0x600083, (0x92018)
    movq $0x800083, (0x92020)
    movq $0xa00083, (0x92028)

    # 打开PAE
    movq %cr4, %rax
    or $0x20, %eax
    movq %rax, %cr4

    # 将页目录基址写入cr3寄存器，假装页目录基址在0x90000处，目前还无法使用虚拟页表
    movq $0x90000, %rax
    movq %rax, %cr3

    # 进入长模式
    mov $0xC0000080, %ecx
    rdmsr

    or $0x100, %eax
    wrmsr

    # 打开保护模式和分页机制
    movq %cr0, %rax
    or $0x1, %eax
    or $0x80000000, %eax
    movq %rax, %cr0

    # 跳转到64位初始化
    movq $setup64, %rax
    pushq $0x8
    pushq %rax
    lretq

setup64:
    # 64位初始化
    cli

    # 加载64位的全局描述符表
    lgdt gdt64desc(%rip)

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

    movq $0xffff800000007e00, %rsp

    # 进入到64位长模式
    movq $kernel_init, %rax
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
    .quad	0x000083	
	.quad	0x200083
	.quad	0x400083
	.quad	0x600083
	.quad	0x800083
    # 显存物理地址，映射到0xa00000的线性地址处
    .quad   0xe0000083
	.fill	506, 8, 0

.p2align 2
# idt64定义
idt64:
    .fill 512, 8, 0

# gdt64定义
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

idt64desc:
    .word 4096 - 1
    .quad idt64

gdt64desc:
    .word gdt64_end - gdt64 - 1     # 段界限
    .quad gdt64                     # 段基址
    