/*
 *  kernel/syscall.c
 *
 *  (C) 2021  Jacky
 */

#include "syscall.h"
#include "stdint.h"
#include "kernel/thread.h"
#include "kernel/console.h"
#include "kernel/panic.h"
#include "fs/fs.h"
#include "fs/file.h"
#include "lib/string.h"

typedef void *syscall_handler;
syscall_handler syscall_table[SYS_BUTT];

pid_t sys_getpid(void)
{
    return Thread_GetRunningTask()->pid;
}

pid_t getpid(void)
{
    return _syscall0(SYS_GETPID);
}

int32_t sys_read(int32_t fd, void *buf, uint32_t count)
{
    ASSERT(buf != NULL);

    if (fd < 0) {
        Console_PutStr("sys_read: fd error\n");
        return -1;
    }

    uint32_t globalFd = File_Local2Global(fd);
    return File_Read(&g_fileTable[globalFd], buf, count);
}

int32_t read(int32_t fd, void *buf, uint32_t count)
{
    return _syscall3(SYS_READ, fd, buf, count);
}

int32_t sys_write(int32_t fd, const void *buf, uint32_t count)
{
    ASSERT(buf != NULL);

    if (fd < 0) {
        Console_PutStr("sys_write: fd error\n");
        return -1;
    }

    if (fd == STDOUT_NO) {
        const char *str = buf;
        Console_PutStr(str);
        return strlen(str);
    }

    uint32_t globalFd = File_Local2Global(fd);
    File *file = &g_fileTable[globalFd];
    if ((file->fdFlag & O_WRONLY) || (file->fdFlag & O_RDWR)) {
        uint32_t bytesWritten = File_Write(file, buf, count);
        return bytesWritten;
    }
    
    /* 没有读写权限 */
    Console_PutStr("sys_write: no allowed to write file\n");
    return -1;
}

/* 重置文件读写偏移指针，成功返回新的偏移量，否则返回-1 */
int32_t sys_lseek(int32_t fd, int32_t offset, FileSeek seekType)
{
    if (fd < 0) {
        Console_PutStr("sys_lseek: fd error\n");
        return -1;
    }

    ASSERT((seekType >= SEEK_SET) && (seekType =< SEEK_END));

    int32_t globalFd = File_Local2Global(fd);
    File *file = &g_fileTable[globalFd];
    int32_t fileSize = (int32_t)file->fdInode->iSize;
    int32_t newPos = 0;
    switch (seekType) {
        case SEEK_SET:
            newPos = offset;
            break;
        
        case SEEK_CUR:
            newPos = (int32_t)file->fdPos + offset;
            break;

        case SEEK_END:
            newPos = fileSize;
            break;
    }

    if ((newPos < 0) || (newPos > fileSize - 1)) {
        Console_PutStr("sys_lseek: offset error. newPos = ");
        Console_PutInt(newPos);
        Console_PutStr("\n");
        return -1;
    }

    file->fdPos = newPos;

    return newPos;
}

int32_t write(int32_t fd, const void *buf, uint32_t count)
{
    return _syscall3(SYS_WRITE, fd, buf, count);
}

void *malloc(uint32_t size)
{
    return _syscall1(SYS_MALLOC, size);
}

void free(void *ptr)
{
    return _syscall1(SYS_FREE, ptr);
}

/* 系统调用模块初始化 */
void Syscall_Init(void)
{
    Console_PutStr("Syscall_Init start.\n");    
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_READ] = sys_read;
    syscall_table[SYS_WRITE] = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE] = sys_free;
    Console_PutStr("Syscall_Init end.\n"); 

    return;
}