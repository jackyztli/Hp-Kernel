/*
 *  Hp-Kernel/mbr/loader.s
 *
 *  (C) 2021  Jacky li
 */
%include "boot.def"
SECTION LOADER vstart=LOADER_BASE_ADDR
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
    mov byte [gs:0x00], '2'
    mov byte [gs:0x01], 0xA4

    mov byte [gs:0x02], ' '
    mov byte [gs:0x03], 0xA4

    mov byte [gs:0x04], 'L'
    mov byte [gs:0x05], 0xA4

    mov byte [gs:0x06], 'O'
    mov byte [gs:0x07], 0xA4

    mov byte [gs:0x08], 'A'
    mov byte [gs:0x09], 0xA4

    mov byte [gs:0x0a], 'D'
    mov byte [gs:0x0b], 0xA4

    mov byte [gs:0x0c], 'E'
    mov byte [gs:0x0d], 0xA4

    mov byte [gs:0x0e], 'R'
    mov byte [gs:0x0f], 0xA4

    jmp $