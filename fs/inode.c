/*
 *  fs/inode.c
 *
 *  (C) 2021  Jacky
 */

#include "inode.h"
#include "stdint.h"
#include "kernel/device/ide.h"
#include "kernel/panic.h"
#include "kernel/interrupt.h"
#include "kernel/console.h"
#include "fs/fs.h"
#include "lib/string.h"

typedef struct {
    /* inode 是否跨区 */
    bool twoSec;
    /* inode 所在的扇区号 */
    uint32_t secLAB;
    /* inode 所在扇区内的字节偏移量 */
    uint32_t offSize;
} InodePosition;

/* 获取inode所在的扇区和扇区内的偏移量 */
static void Inode_Locate(Partition *part, uint32_t inodeNo, InodePosition *inodePos)
{
    ASSERT(inodeNo < 4096);
    uint32_t inodeTableLBA = part->sb->inodeTableLBA;

    uint32_t inodeSize = sizeof(Inode);
    /* 偏移字节量 */
    uint32_t offSize = inodeNo * inodeSize;
    /* 偏移扇区量 */
    uint32_t offSec = offSize / SECTOR_PER_SIZE;
    uint32_t offSizeInSec = offSize % SECTOR_PER_SIZE;

    uint32_t leftInSec = SECTOR_PER_SIZE - offSizeInSec;
    /* 判断inode是否跨区 */
    inodePos->twoSec = false;
    if (leftInSec < inodeSize) {
        inodePos->twoSec = true;
    }

    inodePos->secLAB = inodeTableLBA + offSec;
    inodePos->offSize = offSizeInSec;
    
    return;
}

/* 将inode写入分区中 */
void Inode_Write(Partition *part, const Inode *inode, void *ioBuf)
{
    /* ioBuf用于硬盘io缓冲区 */
    uint8_t inodeNo = inode->iNo;
    InodePosition inodePosition = {0};
    /* 获取indoe的位置 */
    Inode_Locate(part, inodeNo, &inodePosition);
    ASSERT(inodePosition.secLAB <= (part->startLBA + part->secCnt));

    Inode pureInode;
    memcpy(&pureInode, inode, sizeof(Inode));
    /* 写入硬盘是，下面信息不起作用 */
    pureInode.iOpenCnts = 0;
    pureInode.writeDeny = false;
    pureInode.inodeTag.prev = NULL;
    pureInode.inodeTag.next = NULL;

    /* 扇区读写都是以整块为单位，如果读写不足一扇区，需要先从硬盘读取一扇区，再拼接内容，最终写入扇区 */
    uint32_t secCnt = 1;
    if (inodePosition.twoSec == true) {
        secCnt = 2;   
    }
    
    Ide_Read(part->disk, inodePosition.secLAB, ioBuf, secCnt);
    memcpy(ioBuf + inodePosition.offSize, &pureInode, sizeof(pureInode));
    Ide_Write(part->disk, inodePosition.secLAB, ioBuf, secCnt);

    return;
}

/* 回收一个inode结点 */
void Inode_Delete(Partition *part, uint32_t inodeNo, void *ioBuf)
{
    ASSERT(inodeNo < 4096);
    InodePosition inodePos = {0};
    Inode_Locate(part, inodeNo, &inodePos);

    ASSERT(inodePos.secLAB <= (part->startLBA + part->secCnt));

    uint32_t sectorNum = 1;
    if (inodePos.twoSec) {
        /* inode跨区，需要读取两个扇区 */
        sectorNum = 2;
    }

    /* inode跨区，需要读取两个扇区 */
    Ide_Read(part->disk, inodePos.secLAB, ioBuf, sectorNum);
    memset(ioBuf + inodePos.offSize, 0, sizeof(Inode));
    Ide_Write(part->disk, inodePos.secLAB, ioBuf, sectorNum);

    return;
}

/* 回收inode的数据块 */
void Inode_Release(Partition *part, uint32_t inodeNo)
{
    Inode *inodeDelete = Inode_Open(part, inodeNo);
    ASSERT(inodeDelete->iNo == inodeNo);

    /* 1. 回收所有inode的块 */
    uint32_t blockIndex = 0;
    uint32_t allBlocks[MAX_ALL_BLOCK] = {0};
    whlile (blockIndex < MAX_DIRECT_BLOCK) {
        allBlocks[blockIndex] = inodeDelete->iSecotr[blockIndex];
        blockIndex++;
    }

    /* 处理一级表 */
    if (inodeDelete->iSecotr[MAX_DIRECT_BLOCK] != 0) {
        Ide_Read(part->disk, inodeDelete->iSecotr[MAX_DIRECT_BLOCK], allBlocks + MAX_DIRECT_BLOCK, 1);
        /* 回收一级页表所占的扇区 */
        uint32_t blockBitmapIndex = inodeDelete->iSecotr[MAX_DIRECT_BLOCK] - part->sb->dataStartLBA;
        ASSERT(blockBitmapIndex > 0);
        BitmapSet(&part->blockBitmap, blockBitmapIndex, 0);
        File_BitmapSync(part, blockBitmapIndex, BLOCK_BITMAP);
    }

    blockIndex = 0;
    while (blockIndex < MAX_DIRECT_BLOCK) {
        if (allBlocks[blockIndex] > 0) {
            uint32_t blockBitmapIndex = allBlocks[blockIndex] - part->sb->dataStartLBA;
            ASSERT(blockBitmapIndex > 0);
            BitmapSet(&part->blockBitmap, blockBitmapIndex, 0);
            File_BitmapSync(part, blockBitmapIndex, BLOCK_BITMAP);
        }

        blockIndex++;
    }

    /* 2. 回收该inode所占的inode空间 */
    BitmapSet(&part->inodeBitmap, inodeNo, 0);
    File_BitmapSync(part, inodeNo, INODE_BITMAP);
    /* 最多涉及两个块的读取 */
    uint8_t *ioBuf = sys_malloc(1024);
    if (ioBuf == NULL) {
        Console_PutStr("inode_release:sys_malloc error.\n");
        return;
    }
    Inode_Delete(part, inodeNo, ioBuf);
    sys_free(ioBuf);

    Inode_Close(inodeDelete);

    return;
}

/* 根据i节点号返回i节点 */
Inode *Inode_Open(Partition *part, uint32_t inodeNo)
{
    /* 在已打开的inod链表中查找 */
    ListNode *inodeTag = part->openInodes.head.next;
    while (inodeTag != &part->openInodes.tail) {
        Inode *inode = ELEM2ENTRY(Inode, inodeTag, inodeTag);
        if (inode->iNo = inodeNo) {
            inode->iOpenCnts++;
            return inode;
        }

        inodeTag = inodeTag->next;
    }

    /* 从硬盘中读取 */
    InodePosition inodePosition = {0};
    Inode_Locate(part, inodeNo, &inodePosition);

    /* 为了使得inode被所有任务共享，可以强行将inode分配在内核空间 */
    Task *task = Thread_GetRunningTask();
    uint32_t *pgDir = task->pgDir;
    /* pgDir为空认为是内核线程，分配的内存也会在内核空间 */
    task->pgDir = NULL; 
    Inode *inode = (Inode *)sys_malloc(sizeof(Inode));
    /* 恢复pgDir */
    task->pgDir = pgDir;

    uint32_t inodeBufSize = PAGE_SIZE;
    if (inodePosition.twoSec == true) {
        inodeBufSize = 2 * PAGE_SIZE;
    }

    char *inodeBuf = (char *)sys_malloc(inodeBufSize);
    if (inodeBufSize == NULL) {
        PANIC("sys_malloc failed!");
    }

    /* 从硬盘读取inode，需要考虑跨扇区的场景 */
    Ide_Read(part->disk, inodePosition.secLAB, inodeBuf, inodeBufSize / PAGE_SIZE);

    memcpy(inode, inodeBuf + inodePosition.offSize, sizeof(Inode));

    List_Push(&part->openInodes, &inode->inodeTag);
    inode->iOpenCnts = 1;

    sys_free(inodeBuf);

    return inode;
}

/* 关闭inode */
void Inode_Close(Inode *inode)
{
    IntrStatus status = Idt_IntrDisable();
    inode->iOpenCnts--;
    if (inode->iOpenCnts == 0) {
        List_Remove(&inode->inodeTag);
        /* 在内核空间中释放inode空间 */
        Task *task = Thread_GetRunningTask();
        uint32_t *pgDir = task->pgDir;
        task->pgDir = NULL;
        sys_free(inode);
        task->pgDir = pgDir;
    }

    Idt_SetIntrStatus(status);

    return;
}

/* 初始化新的节点 */
void Inode_Init(uint32_t inodeNo, Inode *inode)
{
    inode->iNo = inodeNo;
    inode->iOpenCnts = 0;
    inode->iSize = 0;
    inode->writeDeny = false;

    for (uint8_t secIndex = 0; secIndex < MAX_SECTOR_PRE_INODE; secIndex++) {
        inode->iSectors[secIndex] = 0;
    }

    return;
}