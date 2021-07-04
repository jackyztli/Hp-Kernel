# 这是一个boot程序。
# BIOS程序从启动盘第一个扇区将其读取并放置在0x7c00处。
# CPU将从CS:IP(CS = 0x0, ip = 0x7c00)处开始取指令执行
.globl start
start:
.code16
	# 设置段寄存器
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	
	# 开启A20地址线
	in $0x92, %al
	or $0x2,  %al
	out %al, $0x92
	
	# 加载gdt
	lgdt gdtdesc
	
	# 开启分页基址 
	mov %cr0, %eax
	or  $0x1, %eax
	mov %eax, %cr0
	
	# 远跳转到保护模式
	ljmp $0x8, $p_mode_start

# 保护模式代码
.code32
p_mode_start:
	# 重新设置段选择子，cs寄存器在执行ljmp指令设置成0x8
	movw $0x10, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	movw %ax, %ss
	
	# 栈指针设置成从0x7c00往下
	movl $start, %esp
	call loader_main
	
	# 从loader_main返回就挂起
hang_up:
		jmp hang_up

.p2align 2
# gdt定义
gdt:
	.word 0, 0, 0, 0			# 空段
	.word 0xffff, 0x0000		# 内核代码段
	.word 0x9a00, 0x00cf
	.word 0xffff, 0x0000		# 内核数据段
	.word 0x9200, 0x00cf			
	
gdtdesc:
	.word 0x17			# 段界限
	.long gdt			# 段基址
