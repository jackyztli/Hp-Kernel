/*
 *  fs/file.c
 *
 *  (C) 2021  Jacky
 */

#include "file.h"
#include "kernel/console.h"
#include "kernel/thread.h"
#include "kernel/panic.h"
#include "fs/fs.h"
#include "fs/dir.h"
#include "kernel/device/ide.h"
#include "lib/string.h"

File g_fileTable[MAX_FILE_OPEN];

/* 从文件表g_fileTable中获取一个空闲位，成功则返回下标，失败返回-1 */
int32_t File_GetFreeFd(void)
{
    for (uint32_t fd = 3; fd < MAX_FILE_OPEN; fd++) {
        if (g_fileTable[fd].fdInode == NULL) {
            return fd;
        }
    }

    Console_PutStr("exceed max open files\n");
    return -1;
}

/* 将文件描述符添加到任务中 */
int32_t File_AddFdToTask(int32_t fd)
{
    Task *task = Thread_GetRunningTask();
    for (uint32_t fdIndex = 3; fdIndex < MAX_FILES_OPEN_PER_PROC; fdIndex++) {
        if (task->fdTable[fdIndex] == -1) {
            task->fdTable[fdIndex] = fd;
            return fdIndex;
        }
    }

    Console_PutStr("exceed max open files_per_proc\n");
    return -1;
}

/* 分配一个扇区 */
int32_t File_AllocBlockInBlockBitmap(Partition *part)
{
    int32_t bitIndex = BitmapScan(&part->blockBitmap, 1);
    if (bitIndex == -1) {
        return -1;
    }

    BitmapSet(&part->blockBitmap, bitIndex, 1);
    return (part->sb->dataStartLBA + bitIndex);
}

/* 创建一个文件，成功则返回文件描述符，失败返回-1 */
int32_t File_Create(Dir *parentDir, char *fileName, uint8_t flag)
{
    /* 由于后面会更改系统的bitmap，有可能造成文件创建失败的动作都应该在前面完成 */
    uint8_t *ioBuf = sys_malloc(1024);
    if (ioBuf == NULL) {
        Console_PutStr("sys_malloc failed!!!");
        return -1;
    }

    uint8_t rollbackStep = 0;

    /* 为新文件分配inode */
    int32_t inodeNo = File_AllocBlockInBlockBitmap(g_curPartition);
    if (inodeNo == -1) {
        Console_PutStr("File_AllocBlockInBlockBitmap failed!!!");
        goto rollback;
    }

    /* 需要从堆上申请内存，因为inode要存放在全局数组中 */
    Inode *newFileInode = (Inode *)sys_malloc(sizeof(Inode));
    if (newFileInode == NULL) {
        Console_PutStr("sys_malloc failed!!!");
        rollbackStep = 1;
        /* 组员回滚 */
        goto rollback;
    }

    Inode_Init(inodeNo, newFileInode);

    /* 获取g_fileTable中空闲下标 */
    int32_t fdIndex = File_GetFreeFd();
    if (fdIndex == -1) {
        Console_PutStr("File_GetFreeFd failed!!!");
        rollbackStep = 2;
        goto rollback;
    }

    g_fileTable[fdIndex].fdInode = newFileInode;
    g_fileTable[fdIndex].fdPos = 0;
    g_fileTable[fdIndex].fdFlag = flag;
    g_fileTable[fdIndex].fdInode->writeDeny = false;

    DirEntry newDirEntry = {0};
    /* 创建目录项 */
    Dir_CreateDirEntry(fileName, inodeNo, FT_REGULAR, &newDirEntry);

    /* 同步内存数据到硬盘 */
    
    /* 1. 在目录parentDir下安装目录项newDirEntry */
    if (Dir_SyncDirEntry(parentDir, &newDirEntry, ioBuf) == false) {
        Console_PutStr("Dir_SyncDirEntry failed!!!");
        rollbackStep = 3;
        goto rollback;
    }

    /* 2. 将父目录i结点的内容同步到有硬盘 */
    memset(ioBuf, 0, 1024);
    Inode_Write(g_curPartition, parentDir->inode, ioBuf);

    /* 3. 将创建文件的i结点同步到硬盘 */
    memset(ioBuf, 0, 1024);
    Inode_Write(g_curPartition, newFileInode, ioBuf);

    /* 4. 位图同步到硬盘 */
    memset(ioBuf, 0, 1024);
    File_BitmapSync(g_curPartition, inodeNo, INODE_BITMAP);

    /* 5. 将创建的文件i结点添加在openInodes链表中 */
    List_Push(&g_curPartition->openInodes, &newFileInode->inodeTag);
    newFileInode->iOpenCnts = 1;

    sys_free(ioBuf);

    return File_AddFdToTask(newFileInode->iNo);

/* 创建文件失败，资源回滚 */
rollback:
    switch (rollbackStep) {
    case 3:
        memset(&g_fileTable[fdIndex], 0, sizeof(File));

    case 2:
        sys_free(newFileInode);

    case 1:
        BitmapSet(&g_curPartition->inodeBitmap, inodeNo, 0);
        break;
    }

    sys_free(ioBuf);
    return -1;
}

/* 打开或创建文件系统调用实现，成功返回文件描述符，失败返回-1 */
int32_t sys_open(const char *pathName, uint8_t flags)
{
    /* 本函数只处理文件，不处理目录 */
    if (pathName[strlen(pathName) - 1] == '/') {
        Console_PutStr("can't open a directory ");
        Console_PutStr(pathName);
        Console_PutStr("\n");
        return -1;
    }

    ASSERT(flags <= 7);
    int32_t fd = -1;

    FilePathSearchRecord searchedRecord = {0};

    /* 检查文件是否存在 */
    int32_t inodeNo = FS_SearchFile(pathName, &searchedRecord);

    if (searchedRecord.fileType == FT_DIRECTORY) {
        Console_PutStr("can't open a directory ");
        Console_PutStr(pathName);
        Console_PutStr("\n");

        Dir_Close(searchedRecord.parentDir);

        return -1;
    }

    /* 根据目录深度判断是否所有目录都访问到了 */
    if (FS_PathDepth(searchedRecord.searchedRecord) != FS_PathDepth(pathName)) {
        Console_PutStr("can't access ");
        Console_PutStr(pathName);
        Console_PutStr(": Not a directory, subpath ");
        Console_PutStr(searchedRecord.searchedRecord);
        Console_PutStr(": is't exist\n");
        Dir_Close(searchedRecord.parentDir);
        return -1;
    }

    /* 在最后路径上没找到，并且并不是要创建文件，直接返回-1 */
    if ((inodeNo == -1) && (!(flags & O_CREAT))) {
        Dir_Close(searchedRecord.parentDir);
        return -1;
    } else if ((inodeNo != -1) && (flags & O_CREAT)) {
        /* 要创建的文件已存在 */
        Console_PutStr(pathName);
        Console_PutStr(" has already exist!\n");
        Dir_Close(searchedRecord.parentDir);
        return -1;
    }

    switch (flags & O_CREAT) {
        case O_CREAT:
            Console_PutStr("createing file\n");
            fd = File_Create(searchedRecord.parentDir, strrchr(pathName, '/') + 1, flags);
            Dir_Close(searchedRecord.parentDir);
            break;
    
        default:
            break;
    }

    /* 返回的是pcb里fdTable数组中的元素下标，而不是全局的g_fileTable的下标 */
    return fd;
}

void File_BitmapSync(Partition *part, uint32_t bitIndex, BitmapType bitmapType)
{
    uint32_t offSec = bitIndex / 4096;
    uint32_t offSize = offSec * BLOCK_PER_SIZE;

    uint32_t secLBA;
    uint8_t *bitmapOff;
    switch (bitmapType) {
        case INODE_BITMAP:
            secLBA = part->sb->inodeBitmapLBA + offSec;
            bitmapOff = part->inodeBitmap.bitmap + offSec;
            break;

        case BLOCK_BITMAP:
            secLBA = part->sb->blockBitmapLBA + offSec;
            bitmapOff = part->blockBitmap.bitmap + offSize;
            break;
    }

    Ide_Write(part->disk, secLBA, bitmapOff, 1);

    return;
}