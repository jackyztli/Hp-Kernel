/*
 *  fs/dir.c
 *
 *  (C) 2021  Jacky
 */

#include "dir.h"
#include "kernel/device/ide.h"
#include "kernel/memory.h"
#include "kernel/panic.h"
#include "kernel/console.h"
#include "lib/string.h"
#include "fs/fs.h"
#include "fs/file.h"

Dir g_rootDir;

/* 打开根目录 */
void Dir_OpenRootDir(Partition *part)
{
    g_rootDir.inode = Inode_Open(part, part->sb->rootInodeNo);
    g_rootDir.dirPos = 0;

    return;
}

/* 打开目录 */
Dir *Dir_Open(Partition *part, uint32_t inodeNo)
{
    Dir *dir = (Dir *)sys_malloc(sizeof(Dir));
    if (dir == NULL) {
        PANIC("sys_malloc failed!!!");
        return NULL;
    }

    dir->inode = Inode_Open(part, inodeNo);
    dir->dirPos = 0;

    return dir;
}

/* 关闭目录 */
void Dir_Close(Dir *dir)
{
    if (dir == &g_rootDir) {
        /* 根目录不应被关闭，因为根目录存放在1MB内存之下，并非堆中，不能free */
        return;
    }

    Inode_Close(dir->inode);
    sys_free(dir);

    return;
}

/* 在内存中初始化目录/文件 */
void Dir_CreateDirEntry(const char *fileName, uint32_t inodeNo, uint8_t fileType, DirEntry *dirEntry)
{
    ASSERT(strlen(fileName) <= MAX_FILE_NAME_LEN);

    strcpy(dirEntry->fileName, fileName);
    dirEntry->iNo = inodeNo;
    dirEntry->fileType = fileType;

    return;
}

/* 查找目录或者文件 */
bool Dir_SearchDirEntry(Partition *part, Dir *dir, const char *name, DirEntry *dirEntry)
{   
    /* 12个直接块 + 128个间接块 */
    uint32_t blockCnt = MAX_ALL_BLOCK;

    uint32_t *allBlocks = (uint32_t *)sys_malloc(blockCnt * 4);
    if (allBlocks == NULL) {
        PANIC("sys_malloc failed!!!");
        return false;
    }

    uint32_t blockIndex = 0;
    while (blockIndex < MAX_DIRECT_BLOCK) {
        allBlocks[blockIndex] = dir->inode->iSectors[blockIndex];
        blockIndex++;
    }

    /* 判断是否有一级间接块表 */
    if (dir->inode->iSectors[MAX_DIRECT_BLOCK] != 0) {
        Ide_Read(part->disk, dir->inode->iSectors[MAX_DIRECT_BLOCK], allBlocks + MAX_DIRECT_BLOCK, 1);
    }

    uint8_t *buf = (uint8_t *)sys_malloc(SECTOR_PER_SIZE);
    if (buf == NULL) {
        PANIC("sys_malloc failed!!!");
        return false;
    }

    uint32_t dirEntrySize = part->sb->dirEntrySize;
    /* 一个扇区可容纳的目录项个数 */
    uint32_t dirEntryCnt = SECTOR_PER_SIZE / dirEntrySize;

    blockIndex = 0;
    while (blockIndex < blockCnt) {
        if (allBlocks[blockIndex] == 0) {
            blockIndex++;
            continue;
        }

        memset(buf, 0, SECTOR_PER_SIZE);
        DirEntry *dirEntryTmp = (DirEntry *)buf;
        Ide_Read(part->disk, allBlocks[blockIndex], buf, 1);

        /* 遍历扇区中的所有目录项 */
        uint32_t dirEntryIndex = 0;
        while (dirEntryIndex < dirEntryCnt) {
            if (strcmp(dirEntryTmp->fileName, name) == 0) {
                /* 找到目录或者文件 */
                memcpy(dirEntry, dirEntryTmp, dirEntrySize);
                sys_free(buf);
                sys_free(allBlocks);
                return true;
            }

            dirEntryIndex++;
            dirEntryTmp++;
        }

        blockIndex++;
    }

    sys_free(buf);
    sys_free(allBlocks);

    return false;
}

/* 将目录项dirEntry写入父目录parentDir中 */
bool Dir_SyncDirEntry(Dir *parentDir, DirEntry *dirEntry, void *ioBuf)
{
    Inode *dirInode = parentDir->inode;
    uint32_t dirSize = dirInode->iSize;
    uint32_t dirEntrySize = g_curPartition->sb->dirEntrySize;
    uint32_t dirEntryCnt = (SECTOR_PER_SIZE / dirEntrySize);

    /* 保存所有目录块 */
    uintptr_t allBlocks[MAX_ALL_BLOCK] = {0};

    uint8_t blockIndex = 0;
    while (blockIndex < MAX_DIRECT_BLOCK) {
        allBlocks[blockIndex] = dirInode->iSectors[blockIndex];
        blockIndex++;
    }

    blockIndex = 0;
    while (blockIndex < MAX_ALL_BLOCK) {
        int32_t blockLBA = -1;
        int32_t blockBitmapIndex = -1;
        if (allBlocks[blockIndex] == 0) {
            blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
            if (blockLBA == -1) {
                Console_PutStr("File_AllocBlockInBlockBitmap failed!!!");
                return false;
            }

            blockBitmapIndex = blockLBA - g_curPartition->sb->dataStartLBA;
            ASSERT(blockBitmapIndex != -1);
            File_BitmapSync(g_curPartition, blockBitmapIndex, BLOCK_BITMAP);

            blockBitmapIndex = -1;
            if (blockIndex < MAX_DIRECT_BLOCK) {
                dirInode->iSectors[blockIndex] = blockLBA;
                allBlocks[blockIndex] = blockLBA;
            } else if (blockIndex == MAX_DIRECT_BLOCK) {
                dirInode->iSectors[MAX_DIRECT_BLOCK] = blockLBA;
                blockLBA = -1;
                blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
                if (blockLBA == -1) {
                    blockBitmapIndex = dirInode->iSectors[MAX_DIRECT_BLOCK] - g_curPartition->sb->dataStartLBA;
                    BitmapSet(&g_curPartition->blockBitmap, blockBitmapIndex, 0);
                    dirInode->iSectors[MAX_DIRECT_BLOCK] = 0;
                    Console_PutStr("File_AllocBlockInBlockBitmap for Dir_SyncDirEntry failed!!!");
                    return false;
                }

                blockBitmapIndex = blockLBA - g_curPartition->sb->dataStartLBA;
                ASSERT(blockBitmapIndex != -1);
                File_BitmapSync(g_curPartition, blockBitmapIndex, BLOCK_BITMAP);

                allBlocks[MAX_DIRECT_BLOCK] = blockLBA;
                /* 把新分配的第0个间接块地址写入一级间接块表 */
                Ide_Write(g_curPartition->disk, dirInode->iSectors[MAX_DIRECT_BLOCK], allBlocks + MAX_DIRECT_BLOCK, 1);
            } else {
                allBlocks[blockIndex] = blockLBA;
                Ide_Write(g_curPartition->disk, dirInode->iSectors[MAX_DIRECT_BLOCK], allBlocks + MAX_DIRECT_BLOCK, 1);
            }

            memset(ioBuf, 0, SECTOR_PER_SIZE);
            memcpy(ioBuf, dirEntry, dirEntrySize);
            Ide_Write(g_curPartition->disk, allBlocks[blockIndex], ioBuf, 1);
            dirInode->iSize += dirEntrySize;
            return true;
        }

        DirEntry *dirEntryTmp = (DirEntry *)ioBuf;
        /* 第blockIndex块已经存在，读入内存，查找空目录项 */
        Ide_Read(g_curPartition->disk, allBlocks[blockIndex], ioBuf, 1);
        /* 在扇区内查找空目录项 */
        uint8_t dirEntryIndex = 0;
        while (dirEntryIndex < dirEntryCnt) {
            /* 文件类型FT_UNKNOWN表示未使用或者已经删除 */
            if ((dirEntryTmp + dirEntryIndex)->fileType == FT_UNKNOWN) {
                memcpy(dirEntryTmp + dirEntryIndex, dirEntry, dirEntrySize);
                Ide_Write(g_curPartition->disk, allBlocks[blockIndex], ioBuf, 1);
                dirInode->iSize += dirEntrySize;
                return true;
            }
            dirEntryIndex++;
        }
        blockIndex++;
    }

    Console_PutStr("Directory is full!\n");

    return false;
}