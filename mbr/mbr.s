%include "boot.def"
;执行到这里，硬件默认设置cs为0，ip为0x7c00
SECTION MBR vstart=MBR_BASE_ADDR
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

; 从硬盘读取loader
    mov eax, LOADER_START_SECTOR       ; 读取硬盘LBA值存放在eax
    mov bx, LOADER_BASE_ADDR           ; loader的加载的地址存放在bx
    mov cx, LOADER_SECTOR_NUM          ; loader在硬盘中占的扇区
    call .read_disk_m_16                ; 读取硬盘

; 执行loader程序
    jmp LOADER_BASE_ADDR               ; 执行loader

; 从硬盘读取n个扇区，eax=LBA扇区号，bx是数据写入的内存地址，cx是读取扇区数
.read_disk_m_16:
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
    mov [bx], ax                       ; 读取到ebx内存中
    add bx, 2
    loop .start_to_read_disk
    ret

; 数据定义
    times 510-($-$$) db 0
    db 0x55, 0xaa
