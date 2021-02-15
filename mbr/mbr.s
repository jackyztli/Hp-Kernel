/*
 *  Hp-Kernel/mbr/mbr.s
 *
 *  (C) 2021  Jacky li
 */
;执行到这里，硬件默认设置cs为0，ip为0x7c00
SECTION MBR vstart=0x7c00
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00

; 执行清屏操作，功能号为0x6
    mov ax, 0x600
    mov bx, 0x700
    mov cx, 0
    mov dx, 0x184f
    
    int 0x10

; 获取光标位置，功能号为0x3
    mov ah, 3
    mov bh, 0

    int 0x10

; 打印字符串，功能号为0x13
    mov ax, message
    mov bp, ax
    mov bx, 0x2
    ; 字符串长度
    mov cx, 5
    mov ax, 0x1301 ; al设置成0x01表示光标跟随移动
    mov bx, 0x2
    int 0x10

; 系统不断循环等待
    jmp $

; 数据定义
    message db "1 MBR"
    times 510-($-$$) db 0
    db 0x55, 0xaa