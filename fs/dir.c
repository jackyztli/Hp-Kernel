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

/* 删除目录项，成功返回true，失败返回flase */
bool Dir_DeleteDirEntry(Partition *part, Dir *dir, uint32_t inodeNo, void *ioBuf)
{
    Inode *dirInode = dir->inode;
    uint32_t blockIndex = 0;
    uint32_t allBlocks[MAX_ALL_BLOCK] = {0};

    /* 收集目录项全部地址块 */
    while (blockIndex < MAX_DIRECT_BLOCK) {
        allBlocks[blockIndex] = dirInode->iSectors[blockIndex];
        blockIndex++;
    }

    if (dirInode->iSectors[MAX_DIRECT_BLOCK] != 0) {
        Ide_Read(part->disk, dirInode->iSectors[MAX_DIRECT_BLOCK], ioBuf, 1);
    }

    uint32_t dirEntrySize = part->sb->dirEntrySize;
    uint32_t dirEntryPerSec = (SECTOR_PER_SIZE / dirEntrySize);
    
    blockIndex = 0;
    while (blockIndex < MAX_ALL_BLOCK) {
        if (allBlocks[blockIndex] == 0) {
            blockIndex++;
            continue;
        }

        memset(ioBuf, 0, SECTOR_PER_SIZE);
        /* 读取目录项 */
        Ide_Read(part->disk, allBlocks[blockIndex], ioBuf, 1);

        /* 遍历所有目录项 */
        uint32_t dirEntryIndex = 0;
        DirEntry *foundDirEntry = NULL;
        /* 记录扇区中实际被使用到的目录项 */
        uint32_t dirEntryCnt = 0;
        while (dirEntryIndex < dirEntryPerSec) {
            DirEntry *dirEntry = (DirEntry *)ioBuf + dirEntryIndex;
            if (dirEntry->fileType != FT_UNKNOWN) {
                if (dirEntry->iNo == inodeNo) {
                    /* 找到对应的目录项 */
                    foundDirEntry = dirEntry;
                }
                dirEntryCnt++;
            }

            dirEntryIndex++;
        }

        if (foundDirEntry == NULL) {
            /* 该扇区找不到目录项 */
            blockIndex++;
            continue; 
        }

        /* 找到目的目录项 */
        if (dirEntryCnt == 3) {
            /* 由于目录中包括.和..，故当使用的目录项数为3，表示需要清空块 */
            /* 1、在块位图中回收块 */
            uint32_t blockBitmapIndex = allBlocks[blockIndex] - part->sb->dataStartLBA;
            BitmapSet(&part->blockBitmap, blockBitmapIndex, 0);
            File_BitmapSync(part, blockBitmapIndex, BLOCK_BITMAP);

            /* 2、将块地址从inode中去除 */
            if (blockIndex < MAX_DIRECT_BLOCK) {
                /* 在直接块中，直接去除iSectors中的索引 */
                dirInode->iSectors[blockIndex] = 0;
            } else {
                /* 在间接块中，如果仅有这一块，直接整个间接块回收 */
                bool isNeedRelease = true;
                uint32_t indirectBlockIndex = MAX_DIRECT_BLOCK;
                while (indirectBlockIndex < MAX_DIRECT_BLOCK) {
                    if ((allBlocks[indirectBlockIndex] != 0) && (indirectBlockIndex != blockIndex)) {
                        isNeedRelease = false;
                        break;
                    }
                }

                /* 需要回收间接块 */
                if (isNeedRelease) {
                    blockBitmapIndex = dirInode->iSectors[MAX_DIRECT_BLOCK] - part->sb->dataStartLBA;
                    BitmapSet(&part->blockBitmap, blockBitmapIndex, 0);
                    File_BitmapSync(part, blockBitmapIndex, BLOCK_BITMAP);
                    dirInode->iSectors[MAX_DIRECT_BLOCK] = 0;
                } else {
                    /* 不需要回收间接块 */
                    allBlocks[blockIndex] = 0;
                    Ide_Write(part->disk, allBlocks[MAX_DIRECT_BLOCK], allBlocks + MAX_DIRECT_BLOCK, 1);
                }
            }

        } else {
            /* 不需要清空块 */
            memset(dirEntry, 0, sizeof(DirEntry));
            Ide_Write(part->disk, allBlocks[blockIndex], 1);
        }

        /* 更新目录信息 */
        dirInode->iSize -= dirEntrySize;
        memset(ioBuf, 0, SECTOR_PER_SIZE * 2);
        Inode_Write(part, dirInode, ioBuf);

        return true;
    }

    return false;
}

/* 读取目录，成功返回1个目录项，失败返回NULL */
DirEntry *Dir_Read(Dir *dir)
{
    DirEntry *dirEntry = (DirEntry *)Dir->dirBuf;
    Inode *dirInode = dir->inode;
    uint32_t allBlocks[MAX_ALL_BLOCK] = {0};
    uint32_t blockCnt = MAX_DIRECT_BLOCK;

    uint32_t blockIndex = 0;
    while (blockIndex < MAX_DIRECT_BLOCK) {
        allBlocks[blockIndex] = dirInode->iSectors[blockIndex];
        blockIndex++;
    }

    if (dirInode->iSectors[MAX_DIRECT_BLOCK] != 0) {
        Ide_Read(g_curPartition->disk, dirInode->iSectors[MAX_DIRECT_BLOCK], allBlocks + MAX_DIRECT_BLOCK, 1);
        blockCnt = MAX_ALL_BLOCK;
    }

    blockIndex = 0;
    
    uint32_t dirEntrySize = g_curPartition->sb->dirEntrySize;
    uint32_t dirEntryPerSize = SECTOR_PER_SIZE / dirEntrySize;
    /* 在目录内遍历 */
    while (dir->dirPos < dirInode->iSize) {
        if (dir->dirPos >= dirInode->iSize) {
            return NULL;
        }

        if (allBlocks[blockIndex] == 0) {
            blockIndex++;
            continue;
        }

        memset(dirEntry, 0, SECTOR_PER_SIZE);
        Ide_Read(g_curPartition->disk, allBlocks[blockIndex], dirEntry, 1);
        uint32_t dirEntryIndex = 0;
        uint32_t curDirEntryPos = 0;
        while (dirEntryIndex < dirEntryPerSize) {
            if ((dirEntry + dirEntryIndex)->fileType) {
                if (curDirEntryPos < dir->dirPos) {
                    curDirEntryPos += dirEntrySize;
                    dirEntryIndex++;
                    continue;
                }

                ASSERT(curDirEntryPos == dir->dirPos);
                dir->dirPos += dirEntrySize;
                return dirEntry + dirEntryIndex;
            }
            dirEntryIndex++;
        }
        blockIndex++;
    }

    return NULL;
}

/* 判断目录是否为空 */
bool Dir_IsEmpty(Dir *dir)
{
    Inode *dirInode = dir->inode;
    return (dirInode->iSize == g_curPartition->sb->dirEntrySize * 2);
}

/* 在父目录parentDir中删除childDir */
int32_t Dir_Remove(Dir *parentDir, Dir *chileDir)
{
    Inode *childDirInode = chileDir->inode;
    int32_t blockIndex = 1;
    while (blockIndex <= MAX_DIRECT_BLOCK) {
        ASSERT(childDirInode->iSectors[blockIndex] == 0);
        blockIndex++;
    }

    void *ioBuf = sys_malloc(SECTOR_PER_SIZE * 2);
    if (ioBuf == NULL) {
        Console_PutStr("dir_remove: malloc for ioBuf failed\n");
        return -1;
    }

    /* 在父目录parentDir中删除子目录childDir对应的目录项 */
    Dir_DeleteDirEntry(g_curPartition, parentDir, childDirInode->iNo, ioBuf);

    Inode_Release(g_curPartition, childDirInode->iNo);
    sys_free(ioBuf);

    return 0;
}

