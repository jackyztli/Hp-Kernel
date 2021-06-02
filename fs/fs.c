/*
 *  fs/fs.c
 *
 *  (C) 2021  Jacky
 */

#include "fs.h"
#include "stdint.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "fs/file.h"
#include "kernel/panic.h"
#include "kernel/global.h"
#include "kernel/device/ide.h"
#include "kernel/console.h"
#include "kernel/memory.h"
#include "lib/string.h"

Partition *g_curPartition;

/* 格式化分区 */
static void FS_PartitionFormat(Partition *part)
{
    /* 为方便实现,一个块大小是一扇区 */
    uint32_t bootSectorSects = 1;	  
    uint32_t superBlockSects = 1;
    uint32_t inodeBitmapSects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);	   // I结点位图占用的扇区数.最多支持4096个文件
    uint32_t inodeTableSects = DIV_ROUND_UP((sizeof(Inode) * MAX_FILES_PER_PART), SECTOR_PER_SIZE);
    uint32_t usedSects = bootSectorSects + superBlockSects + inodeBitmapSects + inodeTableSects;
    uint32_t freeSects = part->secCnt - usedSects;

    /************** 简单处理块位图占据的扇区数 ***************/
    uint32_t blockBitmapSects = DIV_ROUND_UP(freeSects, BITS_PER_SECTOR);
    /* block_bitmap_bit_len是位图中位的长度,也是可用块的数量 */
    uint32_t blockBitmapBitLen = freeSects - blockBitmapSects; 
    blockBitmapSects = DIV_ROUND_UP(blockBitmapBitLen, BITS_PER_SECTOR); 
    /*********************************************************/

    /* 超级块初始化 */
    SuperBlock superBlock = {0};
    superBlock.magic = SUPER_BLOCK_MAGIC;
    superBlock.secSnt = part->secCnt;
    superBlock.inodeCnt = MAX_FILES_PER_PART;
    superBlock.partLBABase = part->startLBA;

    superBlock.blockBitmapLBA = superBlock.partLBABase + 2;	 // 第0块是引导块,第1块是超级块
    superBlock.blockBitmapSects = blockBitmapSects;

    superBlock.inodeBitmapLBA = superBlock.blockBitmapLBA + superBlock.blockBitmapSects;
    superBlock.inodeBitmapSects = inodeBitmapSects;

    superBlock.inodeTableLBA = superBlock.inodeBitmapLBA + superBlock.inodeBitmapSects;
    superBlock.inodeTableSects = inodeTableSects; 

    superBlock.dataStartLBA = superBlock.inodeTableLBA + superBlock.inodeTableSects;
    superBlock.rootInodeNo = 0;
    superBlock.dirEntrySize = sizeof(DirEntry);

    Console_PutStr(part->name);
    Console_PutStr(" info:\n");
    Console_PutStr("   magic:0x");
    Console_PutInt(superBlock.magic);
    Console_PutStr("\n");
    Console_PutStr("   partLBABase:0x");
    Console_PutInt(superBlock.partLBABase);
    Console_PutStr("\n");
    Console_PutStr("   all_sectors:0x");
    Console_PutInt(superBlock.secSnt);
    Console_PutStr("\n");
    Console_PutStr("   inodeCnt:0x");
    Console_PutInt(superBlock.inodeCnt);
    Console_PutStr("\n");
    Console_PutStr("   blockBitmapLBA:0x");
    Console_PutInt(superBlock.blockBitmapLBA);
    Console_PutStr("\n");
    Console_PutStr("   blockBitmapSects:0x");
    Console_PutInt(superBlock.blockBitmapSects);
    Console_PutStr("\n");
    Console_PutStr("   inodeBitmapLBA:0x");
    Console_PutInt(superBlock.inodeBitmapLBA);
    Console_PutStr("\n");
    Console_PutStr("   inodeBitmapSects:0x");
    Console_PutInt(superBlock.inodeBitmapSects);
    Console_PutStr("\n");
    Console_PutStr("   inodeTableLBA:0x");
    Console_PutInt(superBlock.inodeTableLBA);
    Console_PutStr("\n");
    Console_PutStr("   inodeTableSects:0x");
    Console_PutInt(superBlock.inodeTableSects);
    Console_PutStr("\n");
    Console_PutStr("   dataStartLBA:0x");
    Console_PutInt(superBlock.dataStartLBA);
    Console_PutStr("\n");

    Disk* hd = part->disk;
    /*******************************
     * 1 将超级块写入本分区的1扇区 *
     ******************************/
    Ide_Write(hd, part->startLBA + 1, &superBlock, 1);
    Console_PutStr("   startLBA:0x");
    Console_PutInt(part->startLBA + 1);
    Console_PutStr("\n");

    /* 找出数据量最大的元信息,用其尺寸做存储缓冲区*/
    uint32_t bufSize = (superBlock.blockBitmapSects >= superBlock.inodeBitmapSects ? superBlock.blockBitmapSects : superBlock.inodeBitmapSects);
    bufSize = (bufSize >= superBlock.inodeTableSects ? bufSize : superBlock.inodeTableSects) * SECTOR_PER_SIZE;
    uint8_t *buf = (uint8_t *)sys_malloc(bufSize);	// 申请的内存由内存管理系统清0后返回
    ASSERT(buf != NULL);

    /**************************************
     * 2 将块位图初始化并写入superBlock.blockBitmapLBA *
     *************************************/
    /* 初始化块位图block_bitmap */
    buf[0] |= 0x01;       // 第0个块预留给根目录,位图中先占位
    uint32_t blockBitmapLastByte = blockBitmapBitLen / 8;
    uint8_t  blockBitmapLastBit  = blockBitmapBitLen % 8;
    uint32_t last_size = SECTOR_PER_SIZE - (blockBitmapLastByte % SECTOR_PER_SIZE);	     // last_size是位图所在最后一个扇区中，不足一扇区的其余部分

    /* 1 先将位图最后一字节到其所在的扇区的结束全置为1,即超出实际块数的部分直接置为已占用*/
    memset(&buf[blockBitmapLastByte], 0xff, last_size);

    /* 2 再将上一步中覆盖的最后一字节内的有效位重新置0 */
    uint8_t bitIdx = 0;
    while (bitIdx <= blockBitmapLastBit) {
        buf[blockBitmapLastByte] &= ~(1 << bitIdx++);
    }
    Ide_Write(hd, superBlock.blockBitmapLBA, buf, superBlock.blockBitmapSects);

    /***************************************
     * 3 将inode位图初始化并写入sb.inodeBitmapLBA *
     ***************************************/
    /* 先清空缓冲区*/
    memset(buf, 0, bufSize);
    buf[0] |= 0x1;      // 第0个inode分给了根目录
    /* 由于inode_table中共4096个inode,位图inode_bitmap正好占用1扇区,
     * 即inode_bitmap_sects等于1, 所以位图中的位全都代表inode_table中的inode,
     * 无须再像block_bitmap那样单独处理最后一扇区的剩余部分,
     * inode_bitmap所在的扇区中没有多余的无效位 */
    Ide_Write(hd, superBlock.inodeBitmapLBA, buf, superBlock.inodeBitmapSects);

    /***************************************
     * 4 将inode数组初始化并写入sb.inodeTableLBA *
     ***************************************/
    /* 准备写inode_table中的第0项,即根目录所在的inode */
    memset(buf, 0, bufSize);  // 先清空缓冲区buf
    Inode* i = (Inode *)buf; 
    i->iSize = superBlock.dirEntrySize * 2;	 // .和..
    i->iNo = 0;   // 根目录占inode数组中第0个inode
    i->iSectors[0] = superBlock.dataStartLBA;	     // 由于上面的memset,i_sectors数组的其它元素都初始化为0 
    Ide_Write(hd, superBlock.inodeTableLBA, buf, superBlock.inodeTableSects);

    /***************************************
     * 5 将根目录初始化并写入sb.dataStartLBA
     ***************************************/
    /* 写入根目录的两个目录项.和.. */
    memset(buf, 0, bufSize);
    DirEntry* p_de = (DirEntry *)buf;

    /* 初始化当前目录"." *
    memcpy(p_de->fileName, ".", 1);
    p_de->iNo = 0;
    p_de->fileType = FT_DIRECTORY;
    p_de++;

    /* 初始化当前目录父目录".." */
    memcpy(p_de->fileName, "..", 2);
    p_de->iNo = 0;   // 根目录的父目录依然是根目录自己
    p_de->fileType = FT_DIRECTORY;

    /* superBlock.data_start_lba已经分配给了根目录,里面是根目录的目录项 */
    Ide_Write(hd, superBlock.dataStartLBA, buf, 1);

    Console_PutStr("   root_dir_lba:0x");
    Console_PutInt(superBlock.dataStartLBA);
    Console_PutStr("\n");

    Console_PutStr(part->name);
    Console_PutStr(" format done");
    Console_PutStr("\n");
    sys_free(buf);

    return;
}

/* 挂载硬盘分区 */
static void FS_Mount(ListNode *partNode, void *arg)
{
    const char *partName = (const char *)arg;
    const Partition *part = ELEM2ENTRY(Partition, partTag, partNode);
    if (strcmp(part->name, partName) == 0) {
        g_curPartition = part;

        SuperBlock superBlock = {0};
        /* 读入超级块 */
        Ide_Read(part->disk, part->startLBA + 1, &superBlock, 1);

        g_curPartition->sb = (SuperBlock *)sys_malloc(sizeof(SuperBlock));
        if (g_curPartition->sb == NULL) {
            PANIC("sys_malloc failed!");
        }

        memcpy(g_curPartition->sb, &superBlock, sizeof(SuperBlock));

        g_curPartition->blockBitmap.bitmap = (uint8_t *)sys_malloc(superBlock.blockBitmapSects * SECTOR_PER_SIZE);
        if (g_curPartition->blockBitmap.bitmap == NULL) {
            PANIC("sys_malloc failed!");
        }
        g_curPartition->blockBitmap.bitmapLen = superBlock.blockBitmapSects * SECTOR_PER_SIZE;
        /* 从硬盘读入位图信息 */
        Ide_Read(part->disk, superBlock.blockBitmapLBA, g_curPartition->blockBitmap.bitmap, superBlock.blockBitmapSects);

        g_curPartition->inodeBitmap.bitmap = (uint8_t *)sys_malloc(superBlock.inodeBitmapSects * SECTOR_PER_SIZE);
        if (g_curPartition->inodeBitmap.bitmap == NULL) {
            PANIC("sys_malloc failed!");
        }
        g_curPartition->blockBitmap.bitmapLen = superBlock.inodeBitmapSects * SECTOR_PER_SIZE;
        /* 从硬盘读入inode位图 */
        Ide_Read(part->disk, superBlock.inodeBitmapLBA, g_curPartition->inodeBitmap.bitmap, superBlock.inodeBitmapSects);

        List_Init(&g_curPartition->openInodes);

        Console_PutStr("mount ");
        Console_PutStr(g_curPartition->name);
        Console_PutStr(" done!\n");
    }

    return;
}

/* 将最上层的路径名称解析出来 */
static char *FS_PathParse(char *pathName, char *nameStore)
{
    if (pathName[0] == '/' ) {
        /* 根目录直接跳过，还要跳过连续多个/的情况，例如 ///ab/c */
        while (*pathName == '/') {
            pathName++;
        }
    }

    while ((*pathName != '/') && (*pathName != 0)) {
        *nameStore = *pathName;
        nameStore++;
        pathName++;
    }

    if (*pathName == 0) {
        return NULL;
    }

    return pathName;
}

/* 返回路径深度 */
int32_t FS_PathDepth(const char *pathName)
{
    ASSERT(pathName != NULL);    

    char name[MAX_FILE_NAME_LEN] = {0};
    char *p = FS_PathParse(pathName, name);

    uint32_t depth = 0;
    while (name[0] != 0) {
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (p != 0) {
            p = FS_PathParse(p, name);
        }
    }

    return depth;
}

/* 搜索文件pathName，若找到则返回其inode号，否则返回-1 */
int32_t FS_SearchFile(const char *pathName, FilePathSearchRecord *searchedRecord)
{
    if ((strcmp(pathName, "/") == 0) || (strcmp(pathName, "/.") == 0) || (strcmp(pathName, "/..") == 0)) {
        /* 查找路径是根目录 */
        searchedRecord->parentDir = &g_rootDir;
        searchedRecord->fileType = FT_DIRECTORY;
        /* 搜索路径为空 */
        searchedRecord->searchedRecord[0] = 0;
        return g_rootDir.inode->iNo;
    }

    uint32_t pathLen = strlen(pathName);
    ASSERT(pathName[0] == '/');
    ASSERT(pathLen > 1);
    ASSERT(pathLen < MAX_PATH_LEN);

    Dir *parentDir = &g_rootDir;
    searchedRecord->parentDir = parentDir;
    searchedRecord->fileType = FT_UNKNOWN;
    /* 父目录的inode号 */
    uint32_t parentInodeNo = 0;

    DirEntry dirEntry = {0};
    /* name记录每次解析出来的路径名称，例如/a/b/c每次name数组的值为"a","b","c" */
    char name[MAX_FILE_NAME_LEN] = {0};
    char *subPath = FS_PathParse(pathName, name);
    while (name[0] != 0) {
        /* 记录查找过的路径，不能超过最大路径长度 */
        ASSERT(strlen(searchedRecord->searchedRecord) < MAX_PATH_LEN);

        strcat(searchedRecord->searchedRecord, "/");
        strcat(searchedRecord->searchedRecord, name);

        memset(&dirEntry, 0, sizeof(dirEntry));
        /* 在所给的目录中查找文件 */
        if (Dir_SearchDirEntry(g_curPartition, parentDir, name, &dirEntry) == true) {
            if (dirEntry.fileType == FT_DIRECTORY) {
                parentInodeNo = parentDir->inode->iNo;
                Dir_Close(parentDir);
                /* 更新父目录 */
                parentDir = Dir_Open(&g_rootDir, dirEntry.iNo);
                searchedRecord->parentDir = parentDir;
            } else if (dirEntry.fileType == FT_REGULAR) {
                searchedRecord->fileType = FT_REGULAR;
                return dirEntry.iNo;
            }
        } else {
            /* 若找不到目录项，要留着parentDir不要关闭，因为如果要创建新文件，要在parentDir中创建 */
            return -1;
        }

        memset(name, 0, MAX_FILE_NAME_LEN);
        if (subPath != NULL) {
            subPath = FS_PathParse(subPath, name);
        }
    }

    /* 执行到这里，表示pathName必然是一个完整路径（不会是文件，如果是文件前面已经返回） */
    Dir_Close(searchedRecord->parentDir);

    searchedRecord->parentDir = Dir_Open(g_curPartition, parentInodeNo);
    searchedRecord->fileType = FT_DIRECTORY;

    return dirEntry.iNo;
}

void FS_Init(void)
{
    Console_PutStr("FS_Init Start.");

    uint8_t channelNo = 0;
    uint8_t partIdx = 0;

    Console_PutStr("searching filesystem......\n");
    while (channelNo < channelCnt) {
        uint8_t devNo = 0;
        while (devNo < MAX_DISK_DEV_NUM) {
            if (devNo == 0) {
                /* 系统启动盘，不做处理 */
                devNo++;
                continue;
            }

            Disk *hd = &g_channels[channelNo].devices[devNo];
            Partition *part = hd->primParts;
            while (partIdx < MAX_DISK_PART_NUM) {
                if (partIdx == MAX_MAIN_PART_NUM) {
                    /* 开始初始化逻辑分区 */
                    part = hd->logicParts;
                }

                /* 分区存在 */
                if (part->secCnt != 0) {
                    /* 从分区读出超级块，通过超级块判断分区是否已经格式化 
                       超级块位于分区第一个扇区(块)，大小为一个扇区(块) */
                    SuperBlock superBlock = {0};
                    Ide_Read(hd, part->startLBA + 1, &superBlock, 1);
                    if (superBlock.magic == SUPER_BLOCK_MAGIC) {
                        Console_PutStr(part->name);
                        Console_PutStr(" has filesystem\n");
                    } else {
                        Console_PutStr(part->name);
                        Console_PutStr("formatting partition......\n");
                        FS_PartitionFormat(part);
                    }
                }
                partIdx++;
                /* 指向下一个分区 */
                part++;
            }
            /* 下一个磁盘 */
            devNo++;
        }
        /* 下一个通道 */
        channelNo++;
    }

    /* 将sdb1分区挂载到系统 */
    List_Traversal(&g_partitionList, FS_Mount, "sdb1");

    /* 打开当前分区根目录 */
    Dir_OpenRootDir(g_curPartition);

    uint32_t fdIndex = 0;
    while (fdIndex < MAX_FILE_OPEN) {
        g_fileTable[fdIndex].fdInode = NULL;
        fdIndex++;
    }

    Console_PutStr("FS_Init End.");

    return;
}

/* 删除文件，成功返回0，失败返回-1 */
int32_t sys_unlink(const char *pathName)
{
    ASSERT(strlen(pathName) < MAX_PATH_LEN);

    /* 检查待删除的文件是否存在 */
    FilePathSearchRecord searchedRecord = {0};
    int32_t inodeNo = FS_SearchFile(pathName);
    ASSERT(inodeNo != 0);
    if (inodeNo == -1) {
        Console_PutStr("file ");
        Console_PutStr(pathName);
        Console_PutStr(" not found!\n");
        Dir_Close(searchedRecord.parentDir);
        return -1;
    }

    if (searchedRecord.fileType == FT_DIRECTORY) {
        Console_PutStr("can't delete a directory with unlink(), use rmdir() to instead\n");
        Dir_Close(searchedRecord.parentDir);
        return -1;
    }

    uint32_t fileIndex = 0;
    while (fileIndex < MAX_FILE_OPEN) {
        if ((g_fileTable[fileIndex].fdInode != NULL) && 
            (g_fileTable[fileIndex].fdInode->iNo == (uint32_t)inodeNo)) {
                break;
            }
        fileIndex++;
    }

    if (fileIndex < MAX_FILE_OPEN) {
        Dir_Close(searchedRecord.parentDir);
        Console_PutStr("file ");
        Console_PutStr(pathName);
        Console_PutStr(" is in use, not allow to delete!\n");
        return -1;
    }

    ASSERT(fileIndex == MAX_FILE_OPEN);

    void *ioBuf = sys_malloc(SECTOR_PER_SIZE + SECTOR_PER_SIZE);
    if (ioBuf == NULL) {
        Dir_Close(searchedRecord.parentDir);
        Console_PutStr("sys_unlink: malloc for ioBuf failed\n");
        return -1;
    }

    Dir *parentDir = searchedRecord.parentDir;
    Dir_DeleteDirEntry(g_curPartition, parentDir, inodeNo, ioBuf);
    Inode_Release(g_curPartition, inodeNo);
    sys_free(ioBuf);
    Dir_Close(searchedRecord.parentDir);

    return 0;
}

/* 创建目录，成功返回0，失败返回-1 */
int32_t sys_mkdir(const char *pathName)
{
    uint8_t rollBackStep = 0;
    void *ioBuf = sys_malloc(SECTOR_PER_SIZE * 2);
    if (ioBuf == NULL) {
        Console_PutStr("sys_mkdir: sys_malloc for ioBuf failed\n");
        return -1;
    }

    FilePathSearchRecord searchedRecord = {0};
    int32_t inodeNo = FS_SearchFile(pathName, &searchedRecord);
    if (inodeNo != -1) {
        Console_PutStr("sys_mkdir: file or directory ");
        Console_PutStr(pathName);
        Console_PutStr(" exist!\n");
        rollBackStep = 1;
        goto rollback;
    } else {
        uint32_t pathNameDepth = FS_PathDepth(pathName);
        uint32_t pathSearchedDepth = FS_PathDepth(searchedRecord.parentDir);
        if (pathNameDepth != pathSearchedDepth) {
            /* 说明并没有访问到全部的路径，某个中间路径不存在 */
            Console_PutStr("sys_mkdir: cannot access ");
            Console_PutStr(pathName);
            Console_PutStr(": Not a directory, subpath ");
            Console_PutStr(searchedRecord.searchedRecord);
            Console_PutStr(" is't exist\n");
            rollBackStep = 1;
            goto rollback;
        }
    }

    Dir *parentDir = searchedRecord.parentDir;
    char *dirName = strrchr(searchedRecord.searchedRecord, '/') + 1;
    inodeNo = File_AllocBlockInBlockBitmap(g_curPartition);
    if (inodeNo == -1) {
        Console_PutStr("sys_mkdir: allocate inode failed\n");
        rollBackStep = 1;
        goto rollback;
    }

    Inode newDirInode = {0};
    Inode_Init(inodeNo, &newDirInode);

    uint32_t blockBitmapIndex = 0;
    uint32_t blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
    if (blockLBA == -1) {
        Console_PutStr("sys_mkdir: alloc block bitmap for create directory failed\n");
        rollBackStep = 2;
        goto rollback;
    }

    newDirInode.iSectors[0] = blockLBA;
    blockBitmapIndex = blockLBA - g_curPartition->sb->dataStartLBA;
    ASSERT(blockBitmapIndex != 0);
    File_BitmapSync(g_curPartition, blockBitmapIndex, BLOCK_BITMAP);

    memset(ioBuf, 0, SECTOR_PER_SIZE * 2);
    DirEntry  *dirEntry = (DirEntry *)ioBuf;

    /* 创建目录时，默认创建.和.. */
    memcpy(dirEntry->fileName, ".", 1);
    dirEntry->iNo = inodeNo;
    dirEntry->fileType = FT_DIRECTORY;

    dirEntry++;

    /* 创建目录时，默认创建.和.. */
    memcpy(dirEntry->fileName, "..", 2);
    dirEntry->iNo = parentDir->inode->iNo;
    dirEntry->fileType = FT_DIRECTORY;
    Ide_Write(g_curPartition->disk, newDirInode.iSectors[0], ioBuf, 1);

    DirEntry newDirEntry = {0};
    Dir_CreateDirEntry(dirName, inodeNo, FT_DIRECTORY, &newDirEntry);
    memset(ioBuf, 0, SECTOR_PER_SIZE * 2);

    if (Dir_SyncDirEntry(parentDir, &newDirEntry, ioBuf) {
        Console_PutStr("sys_mkdir: sync direntry to disk failed\n");
        rollBackStep = 2;
        goto rollback;
    }

    /* 父目录的inode同步到硬盘 */
    memset(ioBuf, 0, SECTOR_PER_SIZE * 2);
    Inode_Write(g_curPartition, parentDir->inode, ioBuf);

    /* 将新创建目录的inode同步到硬盘 */
    memset(ioBuf, 0, SECTOR_PER_SIZE * 2);
    File_BitmapSync(g_curPartition, inodeNo, INODE_BITMAP);

    sys_free(ioBuf);
    Dir_Close(searchedRecord.parentDir);

    return 0;

rollback:
    switch (rollBackStep) {
        case 2:
            BitmapSet(&g_curPartition->inodeBitmap, inodeNo, 0);

        case 1:
            Dir_Close(searchedRecord.parentDir);
            break;
    }

    sys_free(ioBuf);
    return -1;
}

/* 打开目录，成功返回目录指针，失败返回NULL */
Dir *sys_opendir(const char *name)
{
    ASSERT(strlen(name) < MAX_FILE_OPEN);
    if ((name[0] == '/') && ((name[1] == 0) || (name[0] == '.')) {
        return &g_rootDir;
    }

    /* 检查待打开的目录是否存在 */
    FilePathSearchRecord searchedRecord = {0};

    int32_t inodeNo = FS_SearchFile(name, &searchedRecord);
    Dir *ret = NULL;
    if (inodeNo == -1) {
        Console_PutStr("In ");
        Console_PutStr(name);
        Console_PutStr(", sub path ");
        Console_PutStr(searchedRecord.searchedRecord);
        Console_PutStr(" not exist\n");
    } else {
        if (searchedRecord.fileType == FT_REGULAR) {
            Console_PutStr(name);
            Console_PutStr(" is regular file!\n");
        } else if (searchedRecord.fileType == FT_DIRECTORY) {
            ret = Dir_Open(g_curPartition, inodeNo);
        }
    }

    Dir_Close(searchedRecord.parentDir);
    return ret;
}

/* 关闭目录，成功返回0，失败返回-1 */
int32_t sys_closedir(Dir *dir)
{
    int32_t ret = -1;
    if (dir != NULL) {
        Dir_Close(dir);
        ret = 0;
    }

    return ret;
}

/* 读取目录dir中的一个目录项，成功后返回其目录项地址，到目录尾时或出错返回NULL */
DirEntry *sys_readdir(Dir *dir)
{
    ASSERT(dir != NULL);
    return Dir_Read(dir);
}

/* 把目录dir的指针dirPos置0 */
void sys_rewinddir(Dir *dir)
{
    dir->dirPos = 0;
}


/* 删除空目录，成功返回0，失败返回-1 */
int32_t sys_rmdir(const char *pathName)
{
    FilePathSearchRecord searchedRecord = {0};
    int32_t inodeNo = FS_SearchFile(pathName, &searchedRecord);
    ASSERT(inodeNo != 0);
    int32_t retval = -1;
    if (inodeNo == -1) {
        Console_PutStr("In ");
        Console_PutStr(pathName);
        Console_PutStr(", sub path ");
        Console_PutStr(searchedRecord.searchedRecord);
        Console_PutStr(" not exist\n");
    } else {
        if (searchedRecord.fileType == FT_REGULAR) {
            Console_PutStr(pathName);
            Console_PutStr(" is regular file!\n");
        } else {
            Dir *dir = Dir_Open(g_curPartition, inodeNo);
            if (!Dir_IsEmpty(dir)) {
                Console_PutStr("dir ");
                Console_PutStr(pathName);
                Console_PutStr(" is not empty, it is not allowed to delete!\n");
            } else {
                if (!Dir_Remove(searchedRecord.parentDir, dir)) {
                    retval = 0;
                }
            }
            Dir_Close(dir);
        }
    }

    Dir_Close(searchedRecord.parentDir);
    return retval;
}

/* 获得父目录的inode编号 */
static uint32_t FS_GetParentDirInode(uint32_t childInode, void *ioBuf)
{
    Inode *childDirInode = Inode_Open(g_curPartition, childInode);
    uint32_t blockLBA = childDirInode->iSectors[0];
    ASSERT(blockLBA >= g_curPartition->sb->dataStartLBA);
    Inode_Close(childDirInode);
    Ide_Read(g_curPartition->disk, blockLBA, ioBuf, 1);
    DirEntry *dirEntry = (DirEntry *)ioBuf;
    ASSERT((dirEntry[1].iNo < 4096) && (dirEntry[1].fileType == FT_DIRECTORY));
    return dirEntry[1].iNo;
}

static int32_t FS_GetChildDirName(uint32_t parentInode, uint32_t childInode, char *path, void *ioBuf)
{
    Inode *parentDirInode = Inode_Open(g_curPartition, parentInode);
    uint8_t blockIndex = 0;
    uint32_t allBlocks[MAX_ALL_BLOCK] = {0};
    uint32_t blockCnt = MAX_DIRECT_BLOCK;
    whle (blockIndex < MAX_DIRECT_BLOCK) {
        allBlocks[blockIndex] = parentDirInode->iSectors[blockIndex];
        blockIndex++:
    }

    if (parentDirInode->iSectors[MAX_DIRECT_BLOCK]) {
        Ide_Read(g_curPartition->disk, parentDirInode->iSectors[MAX_DIRECT_BLOCK], allBlocks + MAX_DIRECT_BLOCK, 1);
        blockCnt = MAX_ALL_BLOCK;
    }

    Inode_Close(parentDirInode);

    DirEntry *dirEntry = (DirEntry *)ioBuf;
    uint32_t dirEntrySize = g_curPartition->sb->dirEntrySize;
    uint32_t dirEntryPerSize = SECTOR_PER_SIZE / dirEntrySize;

    blockIndex = 0;
    while (blockIndex < blockCnt) {
        if (allBlocks[blockIndex]) {
            Ide_Read(g_curPartition->disk, allBlocks[blockIndex], ioBuf, 1);
            uint8_t dirEntryIndex = 0;
            while (dirEntrySize < dirEntryPerSize) {
                if ((dirEntry + dirEntryIndex)->iNo == childInode) {
                    strcat(path, "/");
                    strcat(path, (DirEntry + dirEntryIndex)->fileName);
                    return 0;
                }
                dirEntryIndex++;
            }
        }
        blockIndex++;
    }

    return -1;
}

/* 获取当前任务工作路径的绝对路径 */
char *sys_getcwd(char *buf, uint32_t size)
{
    ASSERT(buf != NULL);
    void *ioBuf = sys_malloc(SECTOR_PER_SIZE);
    if (ioBuf == NULL) {
        return NULL;
    }

    Task *curTask = Thread_GetRunningTask();
    int32_t parentInode = 0;
    int32_t childInode = curTask->cwdIndoe;
    ASSERT((childInode > 0) && (childInode < 4096));

    if (childInode == 0) {
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    memset(buf, 0, size);
    char fullPathReverse[MAX_PATH_LEN] = {0};

    /* 往上搜索，找到根目录为止 */
    while (childInode) {
        parentInode = FS_GetParentDirInode(childInode, ioBuf);
        if (FS_GetChildDirName(parentInode, childInode, fullPathReverse, ioBuf) == -1) {
            sys_free(ioBuf);
            return NULL;
        }

        childInode = parentInode;
    }

    ASSERT(strlen(fullPathReverse) <= size);

    char *lastSlash;
    while (lastSlash = strrchr(fullPathReverse, '/')) {
        uint16_t len = strlen(buf);
        strcpy(buf + len, lastSlash);
        *lastSlash = NULL;
    }

    sys_free(ioBuf);

    return buf;
}

/* 更改当前工作目录为绝对路径path，成功返回0， 失败返回-1 */
int32_t sys_chdir(const char *path)
{
    int32_t ret = -1;
    FilePathSearchRecord searchedRecord = {0};
    int32_t inodeNo = FS_SearchFile(path, &searchedRecord);
    if (inodeNo != -1) {
        if (searchedRecord.fileType == FT_DIRECTORY) {
            Thread_GetRunningTask()->cwdIndoe = inodeNo;
            ret = 0;
        } else {
            Console_PutStr("sys_chdir: ");
            Console_PutStr(path);
            Console_PutStr(" is regular file or other!");
        }
    }

    Dir_Close(searchedRecord.parentDir);
    return ret;
}

/* 在buf中填充文件结构相关信息，成功返回0，失败返回-1 */
int32_t sys_stat(const char *path, Stat *buf)
{
    if ((!strcmp(path, "/")) || (!strcmp(path, "/.")) || (!strcmp(path, "/.."))) {
        buf->fileType = FT_DIRECTORY;
        buf->iNo = 0;
        buf->size = g_rootDir.inode->iSize;
        return 0;
    }

    int32_t ret = -1;
    FilePathSearchRecord searchedRecord = {0};
    int32_t inodeNo = FS_SearchFile(path, &searchedRecord);
    if (inodeNo != -1) {
        Inode *inode = Inode_Open(g_curPartition, inodeNo);
        buf->size = inode->iSize;
        Inode_Close(inode);
        buf->fileType = searchedRecord.fileType;
        buf->iNo = inodeNo;
        ret = 0;
    } else {
        Console_PutStr("sys_stat: ");
        Console_PutStr(path);
        Console_PutStr(" not found\n");
    }

    Dir_Close(searchedRecord.parentDir);

    return ret;
}
