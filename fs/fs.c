/*
 *  fs/fs.c
 *
 *  (C) 2021  Jacky
 */

#include "fs.h"
#include "stdint.h"
#include "fs/inode.h"
#include "fs/dir.h"
#include "kernel/panic.h"
#include "kernel/global.h"
#include "kernel/device/ide.h"
#include "kernel/console.h"
#include "kernel/memory.h"
#include "lib/string.h"

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
    i->i_size = superBlock.dirEntrySize * 2;	 // .和..
    i->iNo = 0;   // 根目录占inode数组中第0个inode
    i->i_sectors[0] = superBlock.dataStartLBA;	     // 由于上面的memset,i_sectors数组的其它元素都初始化为0 
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

    Console_PutStr("FS_Init End.");

    return;
}