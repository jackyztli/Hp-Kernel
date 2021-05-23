/*
 *  fs/fs.h
 *
 *  (C) 2021  Jacky
 */
#ifndef FS_H
#define FS_H

#include "stdint.h"
#include "kernel/device/ide.h"

/* 每个分区支持最大创建的文件数 */
#define MAX_FILES_PER_PART      4096
/* 每个扇区的字节数 */
#define SECTOR_PER_SIZE         512
/* 每个扇区的位数 */
#define BITS_PER_SECTOR         (SECTOR_PER_SIZE * 8)
/* 块大小 */
#define BLOCK_PER_SIZE          SECTOR_PER_SIZE

/* 最大路径长度 */
#define MAX_PATH_LEN 512

extern Partition *g_curPartition;

/* 文件类型 */
typedef enum {
    /* 未知文件类型 */
    FT_UNKNOWN,
    /* 普通文件 */
    FT_REGULAR,
    /* 目录 */
    FT_DIRECTORY
} FileType;

typedef enum {
    /* 只读 */
    O_RDONLY,
    /* 只写 */
    O_WRONLY,
    /* 读写 */
    O_RDWR,
    /* 创建 */
    O_CREAT = 4
} FileOpenFlag;

typedef struct {
    /* 查找过程中的父路径 */
    char searchedRecord[MAX_PATH_LEN];
    /* 文件或目录所在的直接父目录 */
    struct _Dir *parentDir;
    /* 找到的是普通文件还是目录，找不到将为未知类型FT_UNKNOWN */
    FileType fileType;
} FilePathSearchRecord;

/* 文件系统初始化 */
void FS_Init(void);
/* 返回路径深度 */
int32_t FS_PathDepth(const char *pathName);
/* 搜索文件pathName，若找到则返回其inode号，否则返回-1 */
int32_t FS_SearchFile(const char *pathName, FilePathSearchRecord *searchedRecord);

#endif