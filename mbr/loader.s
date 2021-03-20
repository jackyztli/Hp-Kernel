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

; gdt指针，前2个字节是gdt界限，后4个字节是gdt起始地址
gdt_ptr dw GDT_LIMIT
        dd GDT_BASE

loadermsg db '2 loader in real'

loader_start:
    mov sp, LOADER_STACK_TOP
    mov bp, loadermsg           ; ES:BP 字符串地址
    mov cx, 17                  ; CX 字符串长度
    mov ax, 0x1301              ; AH = 13， AL = 01h
    mov bx, 0x001f              ; 页号为0，1f表示蓝底粉红字
    mov dx, 0x1800
    int 0x10                    ; 利用bios中断号打印

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

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP
    mov ax, SELECTOR_VEDIO
    mov gs, ax

    mov byte [gs:160], 'P'
    
jmp $
