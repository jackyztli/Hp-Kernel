/*
 *  fs/file.c
 *
 *  (C) 2021  Jacky
 */

#include "file.h"
#include "kernel/console.h"
#include "kernel/thread.h"
#include "fs/fs.h"
#include "kernel/device/ide.h"

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
    for (uint32_t fdIndex = 3; fdIndex < MAX_FILE_OPEN; fdIndex++) {
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