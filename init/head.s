# 这是一个head程序。
# 其位于kernel的开头处。
# boot程序将其读取到0x100000处，并执行
.code32
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

    # 打开PAE
    mov %cr4, %eax
    or $0x10, %eax
    mov %eax, %cr4

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

    jmp kernel_init
    hlt

.p2align 2
# idt64定义
idt64:
    .fill 512, 8, 0

# gdt64定义
gdt64:
	.word 0, 0, 0, 0			# 空段
	.word 0x0000, 0x0000		# 内核代码段
	.word 0x9800, 0x0020
	.word 0x0000, 0x0000		# 内核数据段
	.word 0x9200, 0x0000			
	.fill 253, 8, 0

idt64desc:
    .word 4096 - 1
    .quad idt64

gdt64desc:
	.word 0x17			# 段界限
	.long gdt64			# 段基址
