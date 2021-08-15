# 这是一个setup程序，为进入64位CPU做准备。
# 其位于磁盘第2个扇区处，最大占4个扇区。
# loader程序将其读取到0x80000处，并执行
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
    movl $0x91007, (0x90000)
    movl $0x91007, (0x90800)
    movl $0x92007, (0x91000)
    movl $0x000083, (0x92000)
    movl $0x200083, (0x92008)
    movl $0x400083, (0x92010)
    movl $0x600083, (0x92018)
    movl $0x800083, (0x92020)
    movl $0xa00083, (0x92028)

    # 打开PAE
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    # 将页目录基址写入cr3寄存器，假装页目录基址在0x90000处，目前还无法使用虚拟页表
    mov $0x90000, %eax
    mov %eax, %cr3

    # 进入长模式
    mov $0xC0000080, %ecx
    rdmsr

    or $0x100, %eax
    wrmsr

    # 打开保护模式和分页机制
    mov %cr0, %eax
    or $0x1, %eax
    or $0x80000000, %eax
    mov %eax, %cr0

    # 跳转到64位初始化
    mov $prepare_setup64, %eax
    push $0x8
    push %eax
    lret

prepare_setup64:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    jmp 0x100000

# gdt64定义
gdt64:
    .word 0, 0, 0, 0                # 空段
    .quad 0x00209b0000000000        # 内核态代码段
    .quad 0x0000930000000000        # 内核态数据段
    .quad 0x0020f80000000000        # 用户态代码段
    .quad 0x0000f20000000000        # 用户态数据段
    .quad 0x00cf9a000000ffff        # 32位内核态代码段
    .quad 0x00cf92000000ffff        # 32位内核态数据段
gdt64_end:

gdt64desc:
    .word gdt64_end - gdt64 - 1     # 段界限
    .long gdt64                     # 段基址
