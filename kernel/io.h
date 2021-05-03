/*
 *  kernel/io.h
 *
 *  (C) 2021  Jacky
 */
#ifndef IO_H
#define IO_H

#include "stdint.h"

/* 将从端口port读入的一个字节返回 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    __asm__ volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
}

/* 往指定端口写入一个值 */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %b0, %w1" :: "a"(value), "Nd"(port));
}

/* 将从端口port读入的word_cnt个字写入addr */
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) 
{
    /******************************************************
    insw是将从端口port处读入的16位内容写入es:edi指向的内存,
    我们在设置段描述符时, 已经将ds,es,ss段的选择子都设置为相同的值了,
    此时不用担心数据错乱。*/
    __asm__ volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
    /******************************************************/
}

/* 将addr处起始的word_cnt个字写入端口port */
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt)
{
    /*********************************************************
    +表示此限制即做输入又做输出.
    outsw是把ds:esi处的16位的内容写入port端口, 我们在设置段描述符时, 
    已经将ds,es,ss段的选择子都设置为相同的值了,此时不用担心数据错乱。*/
    __asm__ volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
    /******************************************************/
}

#endif