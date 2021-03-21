%include "boot.def"
SECTION LOADER vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR
jmp loader_start

; 构建全局描述符
GDT_BASE: dd 0x00000000
          dd 0x00000000

CODE_DESC: dd 0x0000FFFF
           dd DESC_CODE_HIGH4

DATA_DESC: dd 0x0000FFFF
           dd DESC_DATA_HIGH4

; 显存段界限为：(0xbffff - 0xb8000) / 4k
VEDIO_DESC: dd 0x80000007
            dd DESC_VEDIO_HIGH4

GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1 ; 通过GDT_LIMIT来判断是否越界
times 60 dq 0 ;预留60个段描述符
; 定义段选择子
SELECTOR_CODE equ (0x0001 << 3) + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002 << 3) + TI_GDT + RPL0
SELECTOR_VEDIO equ (0x0003 << 3) + TI_GDT + RPL0

; 记录总物理内存大小
total_mem_bytes dd 0

; gdt指针，前2个字节是gdt界限，后4个字节是gdt起始地址
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

; ards预留244字节空间，用于获取物理内存大小时使用
ards_buf times 244 db 0
ards_nr dw 0

loadermsg db '2 loader in real'

loader_start:
    mov sp, LOADER_STACK_TOP
    mov bp, loadermsg           ; ES:BP 字符串地址
    mov cx, 17                  ; CX 字符串长度
    mov ax, 0x1301              ; AH = 13， AL = 01h
    mov bx, 0x001f              ; 页号为0，1f表示蓝底粉红字
    mov dx, 0x1800
    int 0x10                    ; 利用bios中断号打印

; 获取总物理内存大小，利用bios中断号0x15h, eax = 0000E820h, edx =  534D4150h
    xor ebx, ebx
    mov edx, 0x534d4150
    mov di, ards_buf
; 利用e820中断获取物理内存
.e820_mem_get_loop:
    mov eax, 0x0000e820
    mov ecx, 20                ; ards结构体大小是20字节
    int 0x15
    jc .e820_get_mem_fail_and_try_e801  ; 如果利用e820获取失败，则改用e801
    add di, cx                 ; 指向下一个缓冲区
    inc word [ards_nr]         ; 记录获取到的ards数量
    cmp ebx, 0                 ; 如果ebx为0，表示获取到最后一个结构体
    jnz .e820_mem_get_loop
; 在所有结构体中，找出最大的一组，这个总物理内存容量
    mov cx, [ards_nr]
    mov ebx, ards_buf
    xor edx, edx               ; 清0，用来记录当前最大物理容量

.find_max_mem_area:
    mov eax, [ebx]            ; base_add_low
    add eax, [ebx + 8]        ; length_low
    add ebx, 20               ; 指向下一个缓冲区
    cmp edx, eax
    jge .next_ards
    mov edx, eax              ; 如果比当前记录最大内存要大，则保存

.next_ards:
    loop .find_max_mem_area
    jmp .mem_get_ok          ; 获取成功

; e820获取失败，改成e801
.e820_get_mem_fail_and_try_e801:
    mov ax, 0xe801
    int 0x15
    jc .e801_get_mem_fail_and_try_88
; 获取低15MB大小的容量
    mov cx, 0x400
    mul cx
    shl edx, 16             ; 左移16位
    and eax, 0x0000FFFF
    or edx, eax
    add edx, 0x100000
    mov esi, edx            ; 先把低于15MB的内存容量存入esi寄存器备份

; 获取高16MB大小的容量
    xor eax, eax
    mov ax, bx
    mov ecx, 0x10000
    mul ecx                ; 高32位存入edx，低32位存入eax

    add esi, eax
    mov edx, esi
    jmp .mem_get_ok

; e801获取失败，使用88中断号获取
.e801_get_mem_fail_and_try_88:
    mov ah, 0x88
    int 0x15
    jc .error_hlt
    and eax, 0x0000FFFF
    mov cx, 0x400
    mul cx
    shl edx, 16
    or edx, eax
    add edx, 0x100000

.mem_get_ok:
    mov [total_mem_bytes], edx ; 保存物理内存容量

; 进入保护模式
; 步骤如下：
; 1. 打开A20
in al, 0x92
or al, 0000_0010b
out 0x92, al
; 2. 加载gdt
lgdt [gdt_ptr]
; 3. 将cr0的pe位置为1
mov eax, cr0
or eax, 0x00000001
mov cr0, eax

; 由于刚进入保护模式，流水线中取的指令还是16位的，所以需要刷新
jmp dword SELECTOR_CODE:p_mode_start

.error_hlt:          ; 出错则挂起
    hlt

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VEDIO
    mov gs, ax

; 从启动盘中读入内核程序
    mov eax, KERNEL_START_SECTOR                     ; kernel.bin的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR                    ; kernel.bin存放到内存的位置
    mov ecx, 200

    ; 从启动盘读取kernel.bin到内存
    call .read_disk_m_32

; 创建页目录和页表
    call .setup_page

; 将描述符地址和偏移量写入内存gdt_ptr处，方便后续重新加载
    sgdt [gdt_ptr]

; 将显存段描述符放到0xc0000000往上的空间
    mov ebx, [gdt_ptr + 2]    ; 取得全局描述符基地址
    or dword [ebx + 0x18 + 0x4], 0xc0000000 ; 显存描述符段基址 + 0xc0000000

    ; gdt基址加上0xc0000000，使其成为内核所在的高地址
    add dword [gdt_ptr + 2], 0xc0000000

    add esp, 0xc0000000         ; 栈指针同样需要指向内核地址

; 开启分页机制
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax                ; cr3寄存器存储页目录基址

    ; 打开cr0的pg位
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; 开启分页模式后，重新加载gdt地址
    lgdt [gdt_ptr]

    mov byte [gs:160], 'V'

; 以下进入内核初始化流程
    call .kernel_init
    mov esp, 0xc009f000
    jmp KERNEL_ENTRY

; 内核初始化
.kernel_init:
    xor eax, eax
    xor ebx, ebx        ; ebx记录程序头地址
    xor ecx, ecx        ; cx记录程序头中program header数量
    xor edx, edx        ; dx 记录program header大小，即e_phentsize

    mov dx, [KERNEL_BIN_BASE_ADDR + 42]    ; 偏移文件42字节为e_phentsize，表示program header大小
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28]   ; 偏移文件28字节为e_phoff，表示第一个program header在文件中的偏移量
    add ebx, KERNEL_BIN_BASE_ADDR
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]    ; 偏移文件44字节为e_phnum，表示有几个program header

.each_segment:
    cmp byte [ebx + 0], PT_NULL            ; 若p_type等于0，表示此program header未被使用
    je .next_segment
    ; 将每一个被使用的段拷贝到目的地址（即头信息中的p_vaddr属性）
    ; 实现memcpy(dst, src, size)函数，入栈规则是从右往左
    push dword [ebx + 16]                  ; program header中偏移16字节的地方是p_filesz，压入函数memcpy的第三个参数:size
    mov eax, [ebx + 4]                     ; program header中偏移4字节的地方是p_offset
    add eax, KERNEL_BIN_BASE_ADDR          ; eax为段起始地址
    push eax
    push dword [ebx + 8]                   ; program header中偏移8字节的地方是p_vaddr，压入函数memcpy的第一个参数:dst
    call .memcpy
    add esp, 12                            ; 手动清理参数栈

.next_segment:
    add ebx, edx
    loop .each_segment
    ret

.memcpy:
    cld
    push ebp
    mov ebp, esp
    push ecx                               ; rep指令使用了ecx，需要保存
    mov edi, [ebp + 8]                     ; dst
    mov esi, [ebp + 12]                    ; src
    mov ecx, [ebp + 16]                    ; size
    rep movsb                              ; 逐字节拷贝

    pop ecx
    pop ebp
    ret

; 创建页目录和页表
.setup_page:
    ; 清空页目录表
    mov ecx, 4096
    mov esi, 0
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

; 开始创建页目录项
.create_pde:
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000                ; 第一个页表的位置
    mov ebx, eax                   ; 备份eax，即页表基址

    ; 设置第一个页表的属性
    or eax, PG_US_U | PG_RW_W | PG_P
    ; 设置第一个页目录项
    mov [PAGE_DIR_TABLE_POS + 0x00], eax
    ; 设置第0xc00页目录项
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax
    ; 设置0x00和0xc00都指向第一个页表的原因是在4G的虚拟内存中，0xc0000000往上的空间为内核页表（即0xc00目录项往上的目录项）

    ; 最后一个页目录项指向页目录表本身
    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_POS + 4092], eax

    ; 由于1M低端内存给内核使用（这是由于目前为止，所有代码都运行在1M空间内，方便兼容），此处要为第一个页表的1M / 4k = 256个页表项设置
    mov ecx, 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
; 为第一个页表创建页目录项
.create_pte:
    mov [ebx + esi * 4], edx
    add edx, 4096
    inc esi
    loop .create_pte

; 0xc00往上的目录项均预留给内核使用，现在进行初始化
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000                    ; 第二个页表的位置
    or eax, PG_US_U | PG_RW_W | PG_P   ; 设置第二个页表的属性

    mov ecx, 254                       ; 0xc00往上还有254个页目录项
    mov esi, 0xc04
    mov ebx, PAGE_DIR_TABLE_POS
.create_kernel_pde:
    mov [ebx + esi], eax
    add esi, 4
    add eax, 0x1000
    loop .create_kernel_pde
    ret

; 从硬盘读取n个扇区，eax=LBA扇区号，bx是数据写入的内存地址，cx是读取扇区数
.read_disk_m_32:
    mov esi, eax                       ; 备份eax，后面in/out的操作会修改ax
    mov di, cx                         ; 备份cx

; 1 step：设置读取扇区，端口号为0x1f2
    mov dx, 0x1f2
    mov al, cl
    out dx, al

; 2 step：设置LBA28号，对应端口为0x1f3~0x1f6
    mov eax, esi                       ; 恢复eax

    mov dx, 0x1f3
    out dx, al

    mov cl, 8
    shr eax, cl                        ; eax右移8位
    mov dx, 0x1f4
    out dx, al

    shr eax, cl                        ; eax继续右移8位
    mov dx, 0x1f5
    out dx, al

    ; 0x1f3~0x1f5共24位，还有4位在0x1f6的低4位
    shr eax, cl                        ; eax继续右移8位
    and al, 0x0f                       ; 设置LBA的24~27位
    or al, 0xe0                        ; 将0x1f6的高4位设置成1110，表示使用LBA寻址模式 + 使用主盘
    mov dx, 0x1f6
    out dx, al

; 3 step：向0x1f7端口写入读取命令，即0x20
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

; 4 step：检测硬盘控制器是否已经准备好数据
.is_disk_ready:
    nop                                ; 等效于sleep
    in al, dx                          ; 由于status寄存器也是端口0x1f7，所以此处dx无需重新赋值
    and al, 0x88                       ; 保留第7和第4位，分别带边硬盘是否忙和数据是否已经准备好
    cmp al, 0x08
    jnz .is_disk_ready                 ; 如果数据没准备好，则继续等待

; 5 step：将硬盘数据拷贝到内存
    mov ax, di                         ; 此时di表示要读取的扇区数，即cx
    mov dx, 256                        ; 一次读取2字节，256表示读取一个扇区的次数
    mul dx                             ; 乘法：ax * dx， 结果存放在ax
    mov cx, ax                         ; cx表示读取n个扇区，总的读取次数

    mov dx, 0x1f0                      ; data端口是0x1f0
.start_to_read_disk:
    in ax, dx
    mov [ebx], ax                       ; 读取到ebx内存中
    add ebx, 2
    loop .start_to_read_disk
    ret