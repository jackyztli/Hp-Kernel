/*
 *  fs/file.h
 *
 *  (C) 2021  Jacky
 */
#ifndef FILE_H
#define FILE_H

#include "stdint.h"
#include "fs/inode.h"
#include "kernel/device/ide.h"

typedef struct {
    /* 文件操作偏移地址 */
    uint32_t fdPos;
    /* 文件操作标识，只读，读写等等 */
    uint32_t fdFlag;
    /* 文件关联的inode */
    Inode *fdInode;  
} File;

typedef enum {
    /* 标准输入 */
    STDIN_NO,
    /* 标准输出 */
    STDOUT_NO,
    /* 标准错误 */
    STDERR_NO,
} StdFd;

typedef enum {
    /* inode位图 */
    INODE_BITMAP,
    /* 块位图 */
    BLOCK_BITMAP,
} BitmapType;

/* 系统最大可打开文件数 */
#define MAX_FILE_OPEN 32
/* 分配一个扇区 */
int32_t File_AllocBlockInBlockBitmap(Partition *part);
void File_BitmapSync(Partition *part, uint32_t bitIndex, BitmapType bitmapType);

/* 打开或创建文件系统调用实现，成功返回文件描述符，失败返回-1 */
int32_t sys_open(const char *pathName, uint8_t flags);

#endif