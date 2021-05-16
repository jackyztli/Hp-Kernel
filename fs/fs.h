/*
 *  fs/fs.h
 *
 *  (C) 2021  Jacky
 */
#ifndef FS_H
#define FS_H

/* 每个分区支持最大创建的文件数 */
#define MAX_FILES_PER_PART      4096
/* 每个扇区的字节数 */
#define SECTOR_PER_SIZE         512
/* 每个扇区的位数 */
#define BITS_PER_SECTOR         (SECTOR_PER_SIZE * 8)
/* 块大小 */
#define BLOCK_PER_SIZE          SECTOR_PER_SIZE

/* 文件类型 */
typedef enum {
    /* 未知文件类型 */
    FT_UNKNOWN,
    /* 普通文件 */
    FT_REGULAR,
    /* 目录 */
    FT_DIRECTORY
} FileType;

/* 文件系统初始化 */
void FS_Init(void);

#endif