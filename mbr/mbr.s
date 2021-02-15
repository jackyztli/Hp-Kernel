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
    mov ax, 0xb800
    mov gs, ax

; 执行清屏操作，功能号为0x6
    mov ax, 0x600
    mov bx, 0x700
    mov cx, 0
    mov dx, 0x184f
    
    int 0x10

; 写入打印字符串到缓存
    mov byte [gs:0x00], '1'
    mov byte [gs:0x01], 0xA4

    mov byte [gs:0x02], ' '
    mov byte [gs:0x03], 0xA4
    
    mov byte [gs:0x04], 'M'
    mov byte [gs:0x05], 0xA4
    
    mov byte [gs:0x06], 'B'
    mov byte [gs:0x07], 0xA4

    mov byte [gs:0x08], 'R'
    mov byte [gs:0x09], 0xA4

; 系统不断循环等待
    jmp $

; 数据定义
    times 510-($-$$) db 0
    db 0x55, 0xaa