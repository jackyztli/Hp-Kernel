/*
 *  kernel/device/ide.c
 *
 *  (C) 2021  Jacky
 */

#include "ide.h"
#include "kernel/console.h"
#include "kernel/panic.h"
#include "kernel/global.h"
#include "kernel/interrupt.h"
#include "kernel/io.h"
#include "kernel/device/timer.h"
#include "lib/string.h"

/* 定义硬盘各寄存器的端口号 */
#define reg_data(channel)	 (channel->portBase + 0)
#define reg_error(channel)	 (channel->portBase + 1)
#define reg_sect_cnt(channel)	 (channel->portBase + 2)
#define reg_lba_l(channel)	 (channel->portBase + 3)
#define reg_lba_m(channel)	 (channel->portBase + 4)
#define reg_lba_h(channel)	 (channel->portBase + 5)
#define reg_dev(channel)	 (channel->portBase + 6)
#define reg_status(channel)	 (channel->portBase + 7)
#define reg_cmd(channel)	 (reg_status(channel))
#define reg_alt_status(channel)  (channel->portBase + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)

/* reg_status寄存器的一些关键位 */
#define BIT_STAT_BSY	 0x80	      // 硬盘忙
#define BIT_STAT_DRDY	 0x40	      // 驱动器准备好	 
#define BIT_STAT_DRQ	 0x8	      // 数据传输准备好了

/* device寄存器的一些关键位 */
#define BIT_DEV_MBS	0xa0	    // 第7位和第5位固定为1
#define BIT_DEV_LBA	0x40
#define BIT_DEV_DEV	0x10

/* 一些硬盘操作的指令 */
#define CMD_IDENTIFY	   0xec	    // identify指令
#define CMD_READ_SECTOR	   0x20     // 读扇区指令
#define CMD_WRITE_SECTOR   0x30	    // 写扇区指令

/* 按硬盘数目来确定通道数 */
uint8_t channelCnt;
/* 两个channel通道 */
IdeChannel g_channels[2];
/* 定义可读写的最大扇区数,调试用的 */
/* 当前最大只支持80MB硬盘 */
#define MAX_LBA ((80 * 1024 * 1024 / 512) - 1)

/* 用于记录总扩展分区的起始lba,初始为0,partition_scan时以此为标记 */
int32_t g_extLBABase = 0;
uint8_t g_partitionNo = 0;
uint8_t g_loginNo = 0;	 // 用来记录硬盘主分区和逻辑分区的下标

List g_partitionList;	 // 分区队列

/* 构建一个16字节大小的结构体,用来存分区表项 */
typedef struct {
   uint8_t  bootable;		 // 是否可引导	
   uint8_t  startHead;		 // 起始磁头号
   uint8_t  startSec;		 // 起始扇区号
   uint8_t  startChs;		 // 起始柱面号
   uint8_t  fsType; 		 // 分区类型
   uint8_t  endHead;		 // 结束磁头号
   uint8_t  endSec; 		 // 结束扇区号
   uint8_t  endChs;		     // 结束柱面号
   uint32_t startLBA;		 // 本分区起始扇区的lba地址
   uint32_t secCnt; 		 // 本分区的扇区数目
} PartitionTableEntry __attribute__ ((packed));	 // 保证此结构是16字节大小

/* 引导扇区,mbr或ebr所在的扇区 */
typedef struct {
   uint8_t  other[446];		                    // 引导代码
   PartitionTableEntry partitionTable[4];       // 分区表中有4项,共64字节
   uint16_t signature;		                    // 启动扇区的结束标志是0x55,0xaa,
} BootSector __attribute__ ((packed));

/* 选择读写的硬盘 */
static inline void Ide_SelectDisk(Disk *hd)
{
    uint8_t regDevice = BIT_DEV_MBS | BIT_DEV_LBA;
    /* 若是从盘就置DEV位为1 */
    if (hd->devNo == 1) {
        regDevice |= BIT_DEV_DEV;
    }

    outb(reg_dev(hd->channel), regDevice);

    return;
}

/* 向硬盘控制器写入起始扇区地址及要读写的扇区数 */
static void Ide_SelectSector(Disk* hd, uint32_t lba, uint8_t secCnt)
{
    ASSERT(lba <= MAX_LBA);
    IdeChannel *channel = hd->channel;

    /* 写入要读写的扇区数*/
    outb(reg_sect_cnt(channel), secCnt);	 /* 如果sec_cnt为0,则表示写入256个扇区 */

    /* 写入lba地址(即扇区号) */
    outb(reg_lba_l(channel), lba);		 /* lba地址的低8位,不用单独取出低8位.outb函数中的汇编指令outb %b0, %w1会只用al */
    outb(reg_lba_m(channel), lba >> 8);   /* lba地址的8~15位 */
    outb(reg_lba_h(channel), lba >> 16);  /* lba地址的16~23位 */

    /* 因为lba地址的24~27位要存储在device寄存器的0～3位,
     * 无法单独写入这4位,所以在此处把device寄存器再重新写入一次 */
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->devNo == 1 ? BIT_DEV_DEV : 0) | lba >> 24);

    return;
}

/* 向通道channel发命令cmd */
static inline void Ide_CmdOut(IdeChannel *channel, uint8_t cmd)
{
    /* 只要向硬盘发出了命令便将此标记置为true,硬盘中断处理程序需要根据它来判断 */
    channel->expectingIntr = true;
    outb(reg_cmd(channel), cmd);

    return;
}

/* 硬盘读入sec_cnt个扇区的数据到buf */
static inline void Ide_ReadFromSector(Disk *hd, void *buf, uint8_t secCnt)
{
    uint32_t sizeInByte;
    if (secCnt == 0) {
        /* 因为sec_cnt是8位变量,由主调函数将其赋值时,若为256则会将最高位的1丢掉变为0 */
        sizeInByte = 256 * 512;
    } else { 
        sizeInByte = secCnt * 512; 
    }

    insw(reg_data(hd->channel), buf, sizeInByte / 2);
    
    return;
}

/* 将buf中sec_cnt扇区的数据写入硬盘 */
static inline void Ide_Write2Sector(Disk *hd, void *buf, uint8_t secCnt) 
{
    uint32_t sizeInByte;
    if (secCnt == 0) {
        /* 因为sec_cnt是8位变量,由主调函数将其赋值时,若为256则会将最高位的1丢掉变为0 */
        sizeInByte = 256 * 512;
    } else { 
        sizeInByte = secCnt * 512; 
    }

    outsw(reg_data(hd->channel), buf, sizeInByte / 2);

    return;
}

/* 等待30秒 */
static bool Ide_BusyWait(Disk *hd)
{
    IdeChannel* channel = hd->channel;
    /* 可以等待30000毫秒 */
    uint16_t timeLimit = 30 * 1000;
    while (timeLimit -= 10 >= 0) {
        if (!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
	        return (inb(reg_status(channel)) & BIT_STAT_DRQ);
        } else {
	        /* 睡眠10毫秒 */
            Timer_SleepMTime(10);
        }
    }
    
    return false;
}

/* 从硬盘读取sec_cnt个扇区到buf */
void Ide_Read(Disk *hd, uint32_t lba, void *buf, uint32_t secCnt)
{ 
    ASSERT(lba <= MAX_LBA);
    ASSERT(secCnt > 0);
    Lock_Lock(&hd->channel->lock);

    /* 1 先选择操作的硬盘 */
    Ide_SelectDisk(hd);

    /* 每次操作的扇区数 */
    uint32_t secsOp;
    /* 已完成的扇区数 */
    uint32_t secsDone = 0;
    while(secsDone < secCnt) {
        if ((secsDone + 256) <= secCnt) {
	        secsOp = 256;
        } else {
	        secsOp = secCnt - secsDone;
        }

        /* 2 写入待读入的扇区数和起始扇区号 */
        Ide_SelectSector(hd, lba + secsDone, secsOp);

        /* 3 执行的命令写入reg_cmd寄存器 */
        Ide_CmdOut(hd->channel, CMD_READ_SECTOR);	  
        
        /* 准备开始读数据 */

        /*********************   阻塞自己的时机  ***********************
         在硬盘已经开始工作(开始在内部读数据或写数据)后才能阻塞自己,现在硬盘已经开始忙了,
         将自己阻塞,等待硬盘完成读操作后通过中断处理程序唤醒自己*/
        Lock_P(&hd->channel->diskDone);
        /*************************************************************/

        /* 4 检测硬盘状态是否可读 */
        /* 醒来后开始执行下面代码*/
        if (Ide_BusyWait(hd) != true) {	 
            /* 读失败 */
            Console_PutStr(hd->name);
            Console_PutStr(" read sector ");
            Console_PutInt(lba);
            Console_PutStr(" failed!!!!!!\n");
        }

        /* 5 把数据从硬盘的缓冲区中读出 */
        Ide_ReadFromSector(hd, (void *)((uintptr_t)buf + secsDone * 512), secsOp);
        secsDone += secsOp;
    }

    Lock_UnLock(&hd->channel->lock);

    return;
}

/* 将buf中sec_cnt扇区数据写入硬盘 */
void Ide_Write(Disk * hd, uint32_t lba, void *buf, uint32_t secCnt)
{
    ASSERT(lba <= MAX_LBA);
    ASSERT(secCnt > 0);
    Lock_Lock(&hd->channel->lock);

    /* 1 先选择操作的硬盘 */
    Ide_SelectDisk(hd);

    /* 每次操作的扇区数 */
    uint32_t secsOp;
    /* 已完成的扇区数 */
    uint32_t secsDone = 0;
    while(secsDone < secCnt) {
        if ((secsDone + 256) <= secCnt) {
	        secsOp = 256;
        } else {
	        secsOp = secCnt - secsDone;
        }

        /* 2 写入待写入的扇区数和起始扇区号 */
        Ide_SelectSector(hd, lba + secsDone, secsOp);

        /* 3 执行的命令写入reg_cmd寄存器 */
        Ide_CmdOut(hd->channel, CMD_WRITE_SECTOR);	      
        
        /* 准备开始写数据 */

        /* 4 检测硬盘状态是否可读 */
        if (Ide_BusyWait(hd) != true) {			      
            /* 命令发送失败 */
	        Console_PutStr(hd->name);
            Console_PutStr(" write sector ");
            Console_PutInt(lba);
            Console_PutStr(" failed!!!!!!\n");
        }

        /* 5 将数据写入硬盘 */
        Ide_Write2Sector(hd, (void *)((uintptr_t)buf + secsDone * 512), secsOp);

        /* 在硬盘响应期间阻塞自己 */
        Lock_P(&hd->channel->diskDone);
        secsDone += secsOp;
    }
    
    /* 醒来后开始释放锁*/
    Lock_UnLock(&hd->channel->lock);

    return;
}

/* 将dst中len个相邻字节交换位置后存入buf */
static void Ide_SwapPairsBytes(const char *dst, char *buf, uint32_t len)
{
    uint8_t i;
    for (i = 0; i < len; i += 2) {
        /* buf中存储dst中两相邻元素交换位置后的字符串*/
        buf[i + 1] = *dst++;   
        buf[i]     = *dst++;   
    }
    
    buf[i] = '\0';

    return;
}

/* 获得硬盘参数信息 */
static void Ide_IdentifyDisk(Disk *hd)
{
    char idInfo[512] = {0};
    Ide_SelectDisk(hd);
    Ide_CmdOut(hd->channel, CMD_IDENTIFY);
    /* 向硬盘发送指令后便通过信号量阻塞自己,
     * 待硬盘处理完成后,通过中断处理程序将自己唤醒 */
    Lock_P(&hd->channel->diskDone);

    /* 醒来后开始执行下面代码*/
    if (Ide_BusyWait(hd) == false) {     //  若失败
        Console_PutStr(hd->name);
        Console_PutStr(" identify failed!!!!!!\n");
    }
    Ide_ReadFromSector(hd, idInfo, 1);

    char buf[64] = {0};
    uint8_t snStart = 10 * 2;
    uint8_t snLen = 20;
    uint8_t mdStart = 27 * 2;
    uint8_t mdLen = 40;
    Ide_SwapPairsBytes(&idInfo[snStart], buf, snLen);
    Console_PutStr("   disk ");
    Console_PutStr(hd->name);
    Console_PutStr(" info:\n      SN: ");
    Console_PutStr(buf);
    Console_PutStr("\n");

    memset(buf, 0, sizeof(buf));
    Ide_SwapPairsBytes(&idInfo[mdStart], buf, mdLen);
    Console_PutStr("      MODULE: ");
    Console_PutStr(buf);
    Console_PutStr("\n");

    uint32_t sectors = *(uint32_t*)&idInfo[60 * 2];
    Console_PutStr("      SECTORS: ");
    Console_PutInt(sectors);
    Console_PutStr("      CAPACITY: ");
    Console_PutInt(sectors * 512 / 1024 / 1024);
    Console_PutStr("MB\n");

    return;
}

/* 扫描硬盘hd中地址为extLBA的扇区中的所有分区 */
static void Ide_PartitionScan(Disk *hd, uint32_t extLBA)
{
    BootSector* bs = sys_malloc(sizeof(BootSector));
    Ide_Read(hd, extLBA, bs, 1);
    uint8_t partIdx = 0;
    PartitionTableEntry *p = bs->partitionTable;

    /* 遍历分区表4个分区表项 */
    while (partIdx++ < 4) {
        if (p->fsType == 0x5) {	 // 若为扩展分区
	        if (g_extLBABase != 0) { 
	            /* 子扩展分区的start_lba是相对于主引导扇区中的总扩展分区地址 */
	            Ide_PartitionScan(hd, p->startLBA + g_extLBABase);
	        } else { /* ext_lba_base为0表示是第一次读取引导块,也就是主引导记录所在的扇区 */
	            /* 记录下扩展分区的起始lba地址,后面所有的扩展分区地址都相对于此 */
	            g_extLBABase = p->startLBA;
	            Ide_PartitionScan(hd, p->startLBA);
	        }
        } else if (p->fsType != 0) { /* 若是有效的分区类型 */
	        if (extLBA == 0) {	 /* 此时全是主分区 */
	            hd->primParts[g_partitionNo].startLBA = extLBA + p->startLBA;
	            hd->primParts[g_partitionNo].secCnt = p->secCnt;
	            hd->primParts[g_partitionNo].disk = hd;
	            List_Append(&g_partitionList, &hd->primParts[g_partitionNo].partTag);
                strcpy(hd->primParts[g_partitionNo].name, hd->name);
	            g_partitionNo++;
                /* 0,1,2,3 */
	            ASSERT(g_partitionNo < 4);
	        } else {
	            hd->logicParts[g_loginNo].startLBA = extLBA + p->startLBA;
	            hd->logicParts[g_loginNo].secCnt = p->secCnt;
	            hd->logicParts[g_loginNo].disk = hd;
	            List_Append(&g_partitionList, &hd->logicParts[g_loginNo].partTag);
                /* 逻辑分区数字是从5开始,主分区是1～4. */
                strcpy(hd->logicParts[g_loginNo].name, hd->name);
	            g_loginNo++;
                /* 只支持8个逻辑分区,避免数组越界 */
	            if (g_loginNo >= 8) {
                    return;
                }
            }
        }
        p++;
    }
    sys_free(bs);

    return;
}

/* 打印分区信息
static bool Ide_PartitionInfo(ListNode *p, int arg UNUSED)
{
    uint32_t offset = OFFSET(Partition, partTag);
    Partition *part = (Partition *)((uintptr_t)p - offset);
    Console_PutStr(part->name);
    Console_PutStr(" startLBA:0x");
    Console_PutInt(part->startLBA);
    Console_PutStr(", secCnt:0x");
    Console_PutInt(part->secCnt);
    Console_PutStr("\n");

    /* 在此处return false与函数本身功能无关,
     * 只是为了让主调函数list_traversal继续向下遍历元素 */
//    return false;
//}

/* 硬盘中断处理程序 */
void Ide_IntrHdHandler(uint8_t irqNo)
{
    ASSERT((irqNo == 0x2e) || (irqNo == 0x2f));
    uint8_t chNo = irqNo - 0x2e;
    IdeChannel *channel = &g_channels[chNo];
    ASSERT(channel->irqNo == irqNo);
    /* 不必担心此中断是否对应的是这一次的expectingIntr,
     * 每次读写硬盘时会申请锁,从而保证了同步一致性 */
    if (channel->expectingIntr == true) {
        channel->expectingIntr = false;
        Lock_V(&channel->diskDone);

        /* 读取状态寄存器使硬盘控制器认为此次的中断已被处理,从而硬盘可以继续执行新的读写 */
        inb(reg_status(channel));
    }
}

/* 硬盘数据结构初始化 */
void Ide_Init(void)
{
    Console_PutStr("Ide_Init start.\n");
    /* 系统硬盘数量存放在0x475的位置中 */
    uint8_t hdNums = *((uint8_t *)(0x475));
    uint8_t channelNo = 0;
    uint8_t devNo = 0;
    ASSERT(hdNums > 0);

    channelCnt = DIV_ROUND_UP(hdNums, 2);
    /* 初始化每一个通道 */
    for (uint8_t i = 0; i < channelCnt; i++) {
        IdeChannel *channel = &g_channels[i];
        strcpy(channel->name, "ide");
        /* 为每个ide通道初始化端口基址及中断向量 */
        switch (channelNo) {
	        case 0:
	            channel->portBase	 = 0x1f0;	       /* ide0通道的起始端口号是0x1f0 */
	            channel->irqNo	     = 0x20 + 14;	   /* 从片8259a上倒数第二的中断引脚,温盘,也就是ide0通道的的中断向量号 */
	            break;
	        case 1:
	            channel->portBase	 = 0x170;	       /* ide1通道的起始端口号是0x170 */
	            channel->irqNo   	 = 0x20 + 15;	   /* 从8259A上的最后一个中断引脚,我们用来响应ide1通道上的硬盘中断 */
	            break;
        }

        channel->expectingIntr = false;		           // 未向硬盘写入指令时不期待硬盘的中断
        Lock_Init(&channel->lock);		     
        Lock_Init(&channel->diskDone);

        /* 初始化为0,目的是向硬盘控制器请求数据后,硬盘驱动sema_down此信号量会阻塞线程,
           直到硬盘完成后通过发中断,由中断处理程序将此信号量sema_up,唤醒线程. */
        channel->diskDone.value = 0;

        Idt_RagisterHandler(channel->irqNo, Ide_IntrHdHandler);

        /* 分别获取两个硬盘的参数及分区信息 */
        while (devNo < 2) {
	        Disk *hd = &channel->devices[devNo];
	        hd->channel = channel;
	        hd->devNo = devNo;
            strcpy(hd->name, "sda");
	        Ide_IdentifyDisk(hd);	 /* 获取硬盘参数 */
	        if (devNo != 0) {	     /* 内核本身的裸硬盘(hd60M.img)不处理 */
	            Ide_PartitionScan(hd, 0);  /* 扫描该硬盘上的分区 */  
	        }
	        g_partitionNo = 0;
            g_loginNo = 0;
	        devNo++; 
        }
        devNo = 0;			  	   /* 硬盘驱动器号置0,为下一个channel的两个硬盘初始化。 */
        channelNo++;			   /* 下一个channel */
    }

    Console_PutStr("Ide_Init end.\n");

    return;
}