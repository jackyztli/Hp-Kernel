/*
 *  fs/dir.h
 *
 *  (C) 2021  Jacky
 */
#ifndef DIR_H
#define DIR_H
#include "stdint.h"
#include "fs/inode.h"
#include "fs/fs.h"

/* 最大文件名长度 */
#define MAX_FILE_NAME_LEN 16

/* 目录结构 */
typedef struct {
    Inode* inode;
    /* 记录在目录内的偏移 */
    uint32_t dirPos;
    /* 目录的数据缓存 */
    uint8_t dirBuf[512];
} Dir;

/* 目录项结构 */
typedef struct  {
    /* 普通文件或目录名称 */
    char fileName[MAX_FILE_NAME_LEN];
    /* 普通文件或目录对应的inode编号 */
    uint32_t iNo;
    /* 文件类型 */
    FileType fileType;
} DirEntry;

#endif
