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
#include "kernel/device/ide.h"

/* 最大文件名长度 */
#define MAX_FILE_NAME_LEN 16

/* 目录结构 */
typedef struct {
    Inode *inode;
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

extern Dir g_rootDir;

/* 查找目录或者文件 */
bool Dir_SearchDirEntry(Partition *part, Dir *dir, const char *name, DirEntry *dirEntry);
/* 打开目录 */
Dir *Dir_Open(Partition *part, uint32_t inodeNo);
/* 关闭目录 */
void Dir_Close(Dir *dir);
/* 将目录项dirEntry写入父目录parentDir中 */
bool Dir_SyncDirEntry(Dir *parentDir, DirEntry *dirEntry, void *ioBuf);
/* 打开根目录 */
void Dir_OpenRootDir(Partition *part);
/* 删除目录项，成功返回true，失败返回flase */
bool Dir_DeleteDirEntry(Partition *part, Dir *dir, uint32_t inodeNo, void *ioBuf);
/* 在内存中初始化目录/文件 */
void Dir_CreateDirEntry(const char *fileName, uint32_t inodeNo, uint8_t fileType, DirEntry *dirEntry);
/* 读取目录，成功返回1个目录项，失败返回NULL */
DirEntry *Dir_Read(Dir *dir);
/* 判断目录是否为空 */
bool Dir_IsEmpty(Dir *dir);
/* 在父目录parentDir中删除childDir */
int32_t Dir_Remove(Dir *parentDir, Dir *chileDir);

#endif
