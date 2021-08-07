# 这是一个head程序。
# 其位于kernel的开头处。
# boot程序将其读取到0x100000处，并执行
.code64
.globl setup
setup:
    cli

    # 加载64位全局描述符
    lgdt gdt64desc
    # 加载idt
    #lidt idt64desc

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    # 栈从0x80000开始，页表从0x90000开始
    mov $0x80000, %ebp
    mov %ebp, %esp

    # 打开PAE
    movq %cr4, %rax
    or $0x20, %eax
    movq %rax, %cr4

    # 页表初始化，0xA0000预留给BIOS，页表基址存放在0x90000处是个不错的选择
    # level 4
    movl $0x91007, (0x90000)
    # level 3
    movl $0x92007, (0x91000)
    # level 2
    # 在二级页表中，每个页表项对应2M的物理空间
    movl $0x000083, (0x92000)
    movl $0x200083, (0x92008)
    movl $0x400083, (0x92010)
    movl $0x600083, (0x92018)
    movl $0x800083, (0x92020)
    movl $0xa00083, (0x92018)

    # 将页目录基址写入cr3寄存器
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

    movq $kernel_init, %rax
    pushq $0
    pushq $0x8
    pushq %rax
    lretq

    hlt

.p2align 2
# idt64定义
idt64:
    .fill 512, 8, 0

# gdt64定义
gdt64:
    .word 0, 0, 0, 0                        # 空段
    .word 0xffff, 0x0000            # 内核代码段
    .word 0x9b00, 0x00af
    .word 0xffff, 0x0000            # 内核数据段
    .word 0x9300, 0x00cf
    .fill 253, 8, 0

idt64desc:
    .word 4096 - 1
    .quad idt64

gdt64desc:
    .word 0x17                      # 段界限
    .quad gdt64                     # 段基址
    