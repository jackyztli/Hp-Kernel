[bits 32]
section .text
global Thread_SwitchTo
Thread_SwitchTo:
    ; 保存当前任务的寄存器
    push esi
    push edi
    push ebx
    push ebp
    ; 进行完上述的压栈操作，栈空间布局如下：
    ; nextTask 指针
    ; currTask 指针
    ; Thread_SwitchTo函数的返回地址
    ; esi
    ; edi
    ; ebx
    ; ebp  <-- esp

    ; 获取当前任务的esp指针，并且保存在当前任务的栈
    mov eax, [esp + 20]    ; 获取 currTask 指针
    mov [eax], esp         ; currTask指针指向的Task结构体第一个成员是esp，这里用来保存esp寄存器

    ; 将下一个任务的esp指针设置成当前的esp指针，恢复上下文
    mov eax, [esp + 24]    ; 获取 nextTask 指针
    mov esp, [eax]         ; 恢复下一个任务的esp寄存器，即Task结构体中的taskStack指针指向的地址

    ; 此时，当前CPU已经运行在nextTask的栈中
    ; 以下是恢复下一个任务的寄存器，如果任务是首次被调用，则会设置成0(这是因为初始化的时候将Task结构体中的寄存器值设为0)
    pop ebp
    pop ebx
    pop edi
    pop esi

    ret                    ; 此处很关键，用于设置CPU的eip指针，由于现在已经运行在nextTask栈中，所以此处返回到nextTask任务的Thread_Schedule函数