/*
 *  fs/file.c
 *
 *  (C) 2021  Jacky
 */

#include "file.h"
#include "kernel/console.h"
#include "kernel/thread.h"
#include "kernel/panic.h"
#include "kernel/interrupt.h"
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

/* 打开inode对应的文件，成功则返回文件描述符，失败则返回-1 */
int32_t File_Open(uint32_t inodeNo, uint8_t flags)
{
    int32_t fdIndex = File_GetFreeFd();
    if (fdIndex == -1) {
        Console_PutStr("exceed max open files\n");
        return -1;
    }

    g_fileTable[fdIndex].fdInode = Inode_Open(g_curPartition, inodeNo);
    /* 每次打开文件，要将fdPos还原为0，即让文件内的指针指向开头 */
    g_fileTable[fdIndex].fdPos = 0;
    g_fileTable[fdIndex].fdFlag = flags;
    bool *writeDeny = &g_fileTable[fdIndex].fdInode->writeDeny;

    /* 如果是写文件，则要考虑多个进程同时读写的情况 */
    if ((flags & O_WRONLY) || (flags & O_RDWR)) {
        IntrStatus status = Idt_IntrDisable();
        if (!(*writeDeny)) {
            /* 当前没有其他进程在写该文件，直接占用 */
            *writeDeny = true;
            Idt_SetIntrStatus(status);
        } else {
            Idt_SetIntrStatus(status);
            Console_PutStr("file can't be write now, try again later\n");
            return -1;
        }
    }

    return File_AddFdToTask(fdIndex);
}

/* 关闭文件操作 */
int32_t File_Close(File *file)
{
    if (file == NULL) {
        return -1;
    }

    file->fdInode->writeDeny = false;
    Inode_Close(file->fdInode);
    file->fdInode = NULL;

    return 0;
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
            /* 其余情况均为已存在的文件 */
            fd = File_Open(inodeNo, flags);
            break;
    }

    /* 返回的是pcb里fdTable数组中的元素下标，而不是全局的g_fileTable的下标 */
    return fd;
}

/* 根据进程里的文件描述符id获取全局文件描述符id */
uint32_t File_Local2Global(uint32_t localFd)
{
    Task *task = Thread_GetRunningTask();
    int32_t globalFd = task->fdTable[localFd];
    ASSERT((globalFd >= 0) && (globalFd < MAX_FILE_OPEN));
    return (uint32_t)globalFd;
}

/* 关闭文件系统调用 */
int32_t sys_close(int32_t fd)
{
    if (fd <= 2) {
        return  -1;
    }

    uint32_t globalFd = File_Local2Global(fd);
    /* 任务fdTable表对应项不可用 */
    Thread_GetRunningTask()->fdTable[fd] = -1;
    return File_Close(&g_fileTable[globalFd]);
}

/* 文件内容读取，成功则返回读取文件的长度，失败返回-1 */
int32_t File_Read(File *file, void *buf, uint32_t count)
{
    /* 如果读取的字节数超过文件可读剩余量，则用剩余量作为待读取的字节数 */
    uint32_t needReadSize = (file->fdPos + count) > file->fdInode->iSize ? (file->fdInode->iSize - file->fdPos) : count;
    uint32_t leftReadSize = needReadSize;
    if (leftReadSize == 0) {
        /* 无需读取 */
        return -1;
    }

    uint8_t *ioBuf = sys_malloc(BLOCK_PER_SIZE);
    if (ioBuf == NULL) {
        Console_PutStr("sys_malloc failed!!!\n");
        return -1;
    }

    uint32_t *allBlocks = (uint32_t *)sys_malloc((12 * 4) + BLOCK_PER_SIZE);
    if (allBlocks == NULL) {
        sys_free(ioBuf);
        Console_PutStr("File_Write：sys_malloc failed!!!\n");
        return -1;
    }

    uint32_t startBlock = file->fdPos / BLOCK_PER_SIZE;
    uint32_t endBlock = (file->fdPos + needReadSize) / BLOCK_PER_SIZE;
    uint32_t readBlocks = endBlock - startBlock;

    uint32_t blockIndex = 0;
    uint32_t indirectBlockTable = 0;
    if (readBlocks == 0) {
        /* 不涉及跨区读 */
        if (endBlock < MAX_DIRECT_BLOCK) {
            blockIndex = endBlock;
            allBlocks[blockIndex] = file->fdInode->iSectors[blockIndex];
        } else {
            indirectBlockTable = file->fdInode->iSectors[MAX_DIRECT_BLOCK];
            Ide_Read(g_curPartition->disk, indirectBlockTable, allBlocks + MAX_DIRECT_BLOCK, 1);
        }
    } else {
        /* 读取多个块，有如下三种情况 */
        /* 情况1：起始块和终止块都属于直接块 */
        if (endBlock < MAX_DIRECT_BLOCK) {
            blockIndex = startBlock;
            while (blockIndex <= endBlock) {
                allBlocks[blockIndex] = file->fdInode->iSectors[blockIndex];
                blockIndex++;
            }
        } else if ((startBlock < MAX_DIRECT_BLOCK) && (endBlock >= MAX_DIRECT_BLOCK)) {
            /* 情况2：跨过直接块和间接块读取 */
            blockIndex = startBlock;
            while (blockIndex < MAX_DIRECT_BLOCK) {
                allBlocks[blockIndex] = file->fdInode->iSectors[blockIndex];
                blockIndex++;
            }
            ASSERT(file->fdInode->iSectors[MAX_DIRECT_BLOCK] != 0);
            indirectBlockTable = file->fdInode->iSectors[MAX_DIRECT_BLOCK];
            Ide_Read(g_curPartition->disk, indirectBlockTable, allBlocks + MAX_DIRECT_BLOCK, 1);
        } else {
            /* 情况2：在间接块读取 */
            ASSERT(file->fdInode->iSectors[MAX_DIRECT_BLOCK] != 0);
            indirectBlockTable = file->fdInode->iSectors[MAX_DIRECT_BLOCK];
            Ide_Read(g_curPartition->disk, indirectBlockTable, allBlocks + MAX_DIRECT_BLOCK, 1);
        }
    }

    /* 已经读取的字节数 */
    uint32_t bytesRead = 0;
    uint8_t *bufTemp = (uint8_t *)buf;
    while (bytesRead < needReadSize) {
        uint32_t secIndex = file->fdPos / BLOCK_PER_SIZE;
        uint32_t secLBA = allBlocks[secIndex];
        uint32_t secOffBytes = file->fdPos % BLOCK_PER_SIZE;
        uint32_t secLeftBytes = BLOCK_PER_SIZE - secOffBytes;

        /* 待读入的数据大小 */
        uint32_t chunkSize = leftReadSize < secLeftBytes ? leftReadSize : secLeftBytes;

        memset(ioBuf, 0, BLOCK_PER_SIZE);
        Ide_Read(g_curPartition->disk, secLBA, ioBuf, 1);
        memcpy(bufTemp, ioBuf + secLeftBytes, chunkSize);

        bufTemp += chunkSize;
        file->fdPos += chunkSize;
        bytesRead += chunkSize;
        leftReadSize -= chunkSize;
    }

    sys_free(allBlocks);
    sys_free(ioBuf);

    return bytesRead;
}

/* 将缓冲区的count个字节写入file，成功则返回写入的字节数，否则返回-1 */
int32_t File_Write(File *file, const char *buf, uint32_t count)
{
    /* 一个inode最多支持140个扇区 */
    if ((file->fdInode->iSize + count) > (MAX_SECTOR_PRE_INODE) * 512) {
        /* 超过最大可支持字节数 */
        Console_PutStr("exceed max file size 140 * 512 bytes, write file failed\n");
        return -1;
    }

    /* 文件第一次写 */
    if (file->fdInode->iSectors[0] == 0) {
        int32_t blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
        if (blockLBA == -1) {
            Console_PutStr("File_Write: File_AllocBlockInBlockBitmap failed\n");
            return -1;
        }

        file->fdInode->iSectors[0] = blockLBA;
        File_BitmapSync(g_curPartition, blockLBA - g_curPartition->sb->dataStartLBA, BLOCK_BITMAP);
    }

    uint8_t *ioBuf = sys_malloc(SECTOR_PER_SIZE);
    if (ioBuf == NULL) {
        Console_PutStr("sys_malloc failed!!!\n");
        return -1;
    }

    uint32_t *allBlocks = (uint32_t *)sys_malloc((12 * 4) + BLOCK_PER_SIZE);
    if (allBlocks == NULL) {
        sys_free(ioBuf);
        Console_PutStr("File_Write：sys_malloc failed!!!\n");
        return -1;
    }

    /* 当前已经用的扇区数 */
    uint32_t blocksHasUsed = file->fdInode->iSize / BLOCK_PER_SIZE + 1;
    /* 继续写入count字节后，共使用的扇区数 */
    uint32_t blocksWillUsed = (file->fdInode->iSize + count) / BLOCK_PER_SIZE + 1;
    ASSERT(blocksWillUsed <= MAX_SECTOR_PRE_INODE);

    /* 需要新增的扇区数 */
    uint32_t blocksNeedAdd = blocksWillUsed - blocksHasUsed;

    /* 需要开始写的扇区 */
    uint32_t blockIndex = 0;
    uint32_t indirectBlockTable = 0;
    int32_t blockLBA = -1;
    /* 无需新增扇区 */
    if (blocksNeedAdd == 0) {
        if (blocksWillUsed <= MAX_DIRECT_BLOCK) {
            blockIndex = blocksHasUsed - 1;
            /* 指向最后一个已有数据的扇区 */
            allBlocks[blockIndex] = file->fdInode->iSectors[blockIndex];
        } else {
            /* 需要在间接块中写入数据，这时候间接块应该已经存在 */
            ASSERT(file->fdInode->iSectors[MAX_DIRECT_BLOCK] != 0);
            indirectBlockTable = file->fdInode->iSectors[MAX_DIRECT_BLOCK];
            /* 将间接块读入内存中 */
            Ide_Read(g_curPartition->disk, indirectBlockTable, allBlocks + MAX_DIRECT_BLOCK, 1);
        }
    } else {
        /* 本次写入有增量扇区，需要判断写在直接扇区还是间接扇区 */
        /* 情况1：直接写在直接扇区 */
        if (blocksWillUsed <= MAX_DIRECT_BLOCK) {
            blockIndex = blocksHasUsed - 1;
            ASSERT(file->fdInode->iSectors[blockIndex] != 0);
            allBlocks[blockIndex] = file->fdInode->iSectors[blockIndex];

            /* 再将需要新增的扇区分配好 */
            blockIndex = blocksHasUsed;
            while (blockIndex < blocksWillUsed) {
                blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
                if (blockLBA == -1) {
                    /* 扇区分配失败 */
                    sys_free(ioBuf);
                    sys_free(allBlocks);
                    Console_PutStr("File_Write：alloc block failed!!!\n");
                    return -1;
                }

                ASSERT(file->fdInode->iSectors[blockIndex] == 0);
                file->fdInode->iSectors[blockIndex] = blockLBA;
                allBlocks[blockIndex] = blockLBA;

                /* 同步到硬盘 */
                File_BitmapSync(g_curPartition, blockLBA - g_curPartition->sb->dataStartLBA, BLOCK_BITMAP);
                blockIndex++;
            }
        } else if ((blocksHasUsed <= MAX_DIRECT_BLOCK) && (blocksWillUsed > MAX_DIRECT_BLOCK)) {  
            /* 情况2：旧数据在直接块，新数据在间接块 */
            blockIndex = blocksHasUsed - 1;
            allBlocks[blockIndex] = file->fdInode->iSectors[blockIndex];

            /* 申请间接块 */
            blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
            if (blockLBA == -1) {
                /* 扇区分配失败 */
                sys_free(ioBuf);
                sys_free(allBlocks);
                Console_PutStr("File_Write：alloc block failed!!!\n");
                return -1;
            }

            ASSERT(file->fdInode->iSectors[MAX_DIRECT_BLOCK] == 0);
            file->fdInode->iSectors[MAX_DIRECT_BLOCK] = blockLBA;
            
            blockIndex = blocksHasUsed;
            while (blockIndex < blocksWillUsed) {
                /* 申请块 */
                blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
                if (blockLBA == -1) {
                    /* 扇区分配失败 */
                    sys_free(ioBuf);
                    sys_free(allBlocks);
                    Console_PutStr("File_Write：alloc block failed!!!\n");
                    return -1;
                }

                if (blockIndex < MAX_DIRECT_BLOCK) {
                    /* 直接块 */
                    ASSERT(file->fdInode->iSectors[blockIndex] == 0);
                    file->fdInode->iSectors[blockIndex] = blockLBA;
                    allBlocks[blockIndex] = blockLBA;
                } else {
                    /* 间接块只写allBlocks，最终一次性刷入硬盘 */
                    allBlocks[blockIndex] = blockLBA;
                }

                File_BitmapSync(g_curPartition, blockLBA - g_curPartition->sb->dataStartLBA, 1);
                blockIndex++;
            }
            /* 间接块写入硬盘 */
            Ide_Write(g_curPartition->disk, indirectBlockTable, allBlocks + MAX_DIRECT_BLOCK, 1);
        } else {
            /* 情况3：旧数据已经在间接块 */
            ASSERT(file->fdInode->iSectors[MAX_DIRECT_BLOCK] != 0);
            indirectBlockTable = file->fdInode->iSectors[MAX_DIRECT_BLOCK];

            /* 从硬盘中读入 */
            Ide_Read(g_curPartition->disk, indirectBlockTable, allBlocks + MAX_DIRECT_BLOCK, 1);

            blockIndex = blocksHasUsed;
            while (blockIndex < blocksWillUsed) {
                /* 申请块 */
                blockLBA = File_AllocBlockInBlockBitmap(g_curPartition);
                if (blockLBA == -1) {
                    /* 扇区分配失败 */
                    sys_free(ioBuf);
                    sys_free(allBlocks);
                    Console_PutStr("File_Write：alloc block failed!!!\n");
                    return -1;
                }

                allBlocks[blockIndex] = blockLBA;
                File_BitmapSync(g_curPartition, blockLBA - g_curPartition->sb->dataStartLBA, 1);
                blockIndex++;
            }
            /* 间接块写入硬盘 */
            Ide_Write(g_curPartition->disk, indirectBlockTable, allBlocks + MAX_DIRECT_BLOCK, 1);
        }
    }

    /* 需要用到的块地址都已经添加都allBlocks中 */
    file->fdPos = file->fdInode->iSize - 1;
    /* 写第一块扇区时要注意剩余空间可能不足一块 */
    bool isFirstWrite = true;
    /* 已经写入的数据 */
    uint32_t bytesWritten = 0;
    /* 未写入的数据 */
    uint32_t bytesLeft = count;
    const char *bufTemp = buf;
    while (bytesWritten < count) {
        uint32_t secIndex = file->fdInode->iSize / BLOCK_PER_SIZE;
        uint32_t secLBA = allBlocks[secIndex];
        memset(ioBuf, 0, SECTOR_PER_SIZE);
        if (isFirstWrite) {
            /* 可能不足一个扇区 */
            Ide_Read(g_curPartition->disk, secLBA, ioBuf, 1);
            isFirstWrite = false;
        }

        uint32_t secOffBytes = file->fdInode->iSize % BLOCK_PER_SIZE;
        uint32_t secLeftBytes = BLOCK_PER_SIZE - secOffBytes;
        /* 本次要写入的字节数 */
        uint32_t chunkSize = bytesLeft > secLeftBytes ? secLeftBytes : bytesLeft;
        memcpy(ioBuf + secOffBytes, bufTemp, chunkSize);
        Ide_Write(g_curPartition->disk, secLBA, ioBuf, 1);

        Console_PutStr("file write at lba 0x");
        Console_PutInt(secLBA);
        Console_PutStr("\n");

        bufTemp += chunkSize;
        bytesWritten += chunkSize;
        bytesLeft -= bytesLeft;
        file->fdInode->iSize += chunkSize;
        file->fdPos += chunkSize;
    }

    /* file的inode信息已经更新，重新写回到硬盘中 */
    Inode_Write(g_curPartition, file->fdInode, ioBuf);
    sys_free(ioBuf);
    sys_free(allBlocks);

    return bytesWritten;
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