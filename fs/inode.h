/*
 *  fs/inode.h
 *
 *  (C) 2021  Jacky
 */
#ifndef INODE_H
#define INODE_H

#include "stdint.h"
#include "lib/list.h"

#define SUPER_BLOCK_MAGIC 0x20210515

/* 超级块定义 */
typedef struct {
    uint32_t magic;		    // 用来标识文件系统类型,支持多文件系统的操作系统通过此标志来识别文件系统类型
    uint32_t secSnt;		    // 本分区总共的扇区数
    uint32_t inodeCnt;		    // 本分区中inode数量
    uint32_t partLBABase;	    // 本分区的起始lba地址

    uint32_t blockBitmapLBA;	    // 块位图本身起始扇区地址
    uint32_t blockBitmapSects;     // 扇区位图本身占用的扇区数量

    uint32_t inodeBitmapLBA;	    // i结点位图起始扇区lba地址
    uint32_t inodeBitmapSects;	    // i结点位图占用的扇区数量

    uint32_t inodeTableLBA;	    // i结点表起始扇区lba地址
    uint32_t inodeTableSects;	    // i结点表占用的扇区数量

    uint32_t dataStartLBA;	    // 数据区开始的第一个扇区号
    uint32_t rootInodeNo;	    // 根目录所在的I结点号
    uint32_t dirEntrySize;	    // 目录项大小

    uint8_t  pad[460];		    // 加上460字节,凑够512字节1扇区大小
} __attribute__((packed)) SuperBlock;

/* inode结构 */
typedef struct {
    uint32_t iNo;          // inode编号

    /* 当此inode是文件时,i_size是指文件大小,
       若此inode是目录,i_size是指该目录下所有目录项大小之和 */
    uint32_t i_size;
    uint32_t i_open_cnts;   // 记录此文件被打开的次数
    bool write_deny;	    // 写文件不能并行,进程写文件前检查此标识
    uint32_t i_sectors[13]; /* i_sectors[0-11]是直接块, i_sectors[12]用来存储一级间接块指针 */
    ListNode inode_tag;
} Inode;

#endif