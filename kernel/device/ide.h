/*
 *  kernel/device/ide.h
 *
 *  (C) 2021  Jacky
 */
#ifndef IDE_H
#define IDE_H

#include "stdint.h"
#include "kernel/sync.h"
#include "kernel/bitmap.h"

/* 系统支持最大硬盘设备数 */
#define MAX_DISK_DEV_NUM 2
/* 系统支持最大主分区数 */
#define MAX_MAIN_PART_NUM 4
/* 系统支持最大逻辑分区数 */
#define MAX_LOGIN_PART_NUM 8
/* 系统支持最大分区数，4个主分区 + 8个逻辑分区 */
#define MAX_DISK_PART_NUM (MAX_MAIN_PART_NUM + MAX_LOGIN_PART_NUM)

/* 分区结构 */
typedef struct {
    uint32_t startLBA;          /* 起始扇区 */
    uint32_t secCnt;            /* 扇区数 */
    struct _Disk *disk;         /* 分区所属的硬盘 */
    ListNode partTag;           /* 用于队列中的标记 */
    char name[8];               /* 分区名 */
    struct _SuperBlock *sb;     /* 超级块 */
    Bitmap blockBitmap;         /* 块位图 */
    Bitmap inodeBitmap;         /* i结点位图 */
    List openInodes;            /* 本分区打开的i结点队列 */   
} Partition;

/* 硬盘结构 */
typedef struct _Disk {
    char name[8];                  /* 本硬盘名 */
    struct _IdeChannel *channel;   /* 本硬盘归属的通道 */
    uint8_t devNo;                 /* 本硬盘是主还是从 */
    Partition primParts[MAX_MAIN_PART_NUM];        /* 主分区最多支持4个 */
    Partition logicParts[MAX_LOGIN_PART_NUM];       /* 逻辑分区最多支持8个 */
} Disk;

/* ata通道结构 */
typedef struct _IdeChannel {
    char name[8];           /* 本ata通道名 */
    uint16_t portBase;      /* 本ata通道起始端口号 */
    uint16_t irqNo;         /* 本通道所用的中断号 */
    Lock lock;              /* 通道锁 */
    bool expectingIntr;     /* 表示等到硬盘的中断 */
    Lock diskDone;          /* 读写硬盘时由线程阻塞自己，等到读写结束后由中断唤醒 */
    Disk devices[2];        /* 一个通道上连接连个硬盘，一主一从 */
} IdeChannel;

extern uint8_t channelCnt;
extern IdeChannel g_channels[2];
extern List g_partitionList;	 // 分区队列

/* 从硬盘读取sec_cnt个扇区到buf */
void Ide_Read(Disk *hd, uint32_t lba, void *buf, uint32_t secCnt);
/* 将buf中sec_cnt扇区数据写入硬盘 */
void Ide_Write(Disk * hd, uint32_t lba, void *buf, uint32_t secCnt);
/* 硬盘数据结构初始化 */
void Ide_Init(void);

#endif