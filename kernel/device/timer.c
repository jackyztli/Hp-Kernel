/*
 *  kernel/device/timer.c
 *
 *  (C) 2021  Jacky
 */

#include "timer.h"
#include "stdint.h"
#include "kernel/io.h"
#include "lib/print.h"

#define COUNTER0_PORT      0x40
#define PIT_CONTROL_PORT   0x43
#define INPUT_FREQUENCY    1193180
#define IRQ0_FREQUENCY     100
#define COUNTER0_FREQUENCY (INPUT_FREQUENCY / IRQ0_FREQUENCY)

/* 设置时钟中断周期 */
static inline void Timer_SetFrequency(uint16_t timerFrequency)
{
    /* 往0x43控制字寄存器端口写入控制字 */
    /* 0 << 6表示给0号计数器赋值 
     * 3 << 4表示计数器读写锁属性
     * 2 << 1表示使用模式2
     */
    outb(PIT_CONTROL_PORT, (uint8_t)((0 << 6) | (3 << 4) | (2 << 1)));

    /* 将计数器周期写入0x40端口 */
    /* 先写入低8位 */
    outb(COUNTER0_PORT, (uint8_t)timerFrequency);
    /* 再写入高8位 */
    outb(COUNTER0_PORT, (uint8_t)(timerFrequency >> 8));
}

/* 初始化8253时钟 */
void Timer_Init(void)
{
    put_str("Timer_Init start. \n");

    /* 设置时钟中断周期为每秒100次中断 */
    Timer_SetFrequency(COUNTER0_FREQUENCY);

    put_str("Timer_Init end. \n");
}