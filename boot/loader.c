/* 这是loader源码，运行在保护模式下
 * 主要功能是从硬盘中读取kernel程序，并调用kernel的入口函数
 */

#include <stdint.h>
#include <x86.h>
#include <elf.h>

#define SECTOR_SIZE 512
#define KERNEL_START_SECTOR 2
#define KERNEL_START_ADDR 0x10000

/* 等待硬盘可读 */
static void waitdisk(void)
{
    while ((inb(0x1f7) & 0xC0) != 0x40) {

    }
}

/* 读取一个扇区到内存 */
void read_sector(void *dst, uint32_t secno)
{
    /* 等待硬盘可读 */
    waitdisk();

    /* 写入硬盘参数 */
    outb(0x1f2, 1);                 /* 读取一个扇区 */
    /* 写入硬盘读取扇区LBA */
    outb(0x1f3, secno & 0xff);
    outb(0x1f4, (secno >> 8) & 0xff);
    outb(0x1f5, (secno >> 16) & 0xff);
    outb(0x1f6, ((secno >> 24) & 0xff) | 0xe0);
    outb(0x1f7, 0x20);

    /* 等待硬盘可读 */
    waitdisk();

    /* 循环读取端口数据 */
    insl(0x1f0, dst, SECTOR_SIZE / 4);
}

void loader_main(void)
{
    /* kernel程序放置到64KB的起始地址处 */
    elfhdr *elf = (elfhdr *)KERNEL_START_ADDR;

    /* 读取kernel程序 */
    read_sector(elf, KERNEL_START_SECTOR);

    /* 校验魔数字 */
    if (*(uint32_t *)elf->ident != ELF_MAGIC) {
        return;
    }

    /* 拷贝段 */
    elfphdr *phdr = (elfphdr *)((void *)elf + elf->phoff);
    elfphdr *endphdr = (elfphdr *)(phdr + elf->phnum);
    for (; phdr < endphdr; phdr++) {
        uint32_t secno = (phdr->offset / SECTOR_SIZE) + KERNEL_START_SECTOR;
        uintptr_t start = (phdr->vaddr & 0xffffff) - (phdr->offset % SECTOR_SIZE);
        uintptr_t end = (phdr->vaddr & 0xffffff) + phdr->memsz;
        for (; start < end; start += SECTOR_SIZE, secno++) {
            read_sector((void *)start, secno);
        }
    }

    /* 调用head程序入口函数 */
    ((void (*)(void))((uintptr_t)elf->entry))();

    while (1) {

    }
}