/*
 *  kernel/device/timer.c
 *
 *  (C) 2021  Jacky
 */

#include "timer.h"
#include "stdint.h"
#include "kernel/io.h"
#include "kernel/interrupt.h"
#include "kernel/thread.h"
#include "kernel/global.h"
#include "lib/print.h"

#define COUNTER0_PORT      0x40
#define PIT_CONTROL_PORT   0x43
#define INPUT_FREQUENCY    1193180
#define IRQ0_FREQUENCY     100
#define COUNTER0_FREQUENCY (INPUT_FREQUENCY / IRQ0_FREQUENCY)

static uint32_t g_sysTicks = 0;

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

/* 时钟中断处理函数 */
static void Timer_IntrHandler(void)
{
    /* 获取当前正在运行的任务 */
    Task *currTask = Thread_GetRunningTask();

    currTask->elapsedTicks++;

    /* 系统ticks加1 */
    g_sysTicks++;

    if (currTask->ticks == 0) {
        /* CPU时间已经用完，进行任务调度 */
        Thread_Schedule();
    } else {
        currTask->ticks--;
    }
}

/* 以tick位单位的sleep */
static void Timer_SleepTicks(uint32_t ticks)
{
    uint32_t startTicks = g_sysTicks;
    while (g_sysTicks - startTicks < ticks) {
        /* sleep时间未到，继续让出CPU使用权 */
        Thread_Yield();
    }
}

/* 以毫秒为单位sleep */
void Timer_SleepMTime(uint32_t mSeconds)
{
    uint32_t ticks = DIV_ROUND_UP(mSeconds, (1000 / IRQ0_FREQUENCY));
    Timer_SleepTicks(ticks);
}

/* 初始化8253时钟 */
void Timer_Init(void)
{
    put_str("Timer_Init start. \n");

    /* 设置时钟中断周期为每秒100次中断 */
    Timer_SetFrequency(COUNTER0_FREQUENCY);

    /* 注册时钟中断处理函数 */
    Idt_RagisterHandler(0x20, Timer_IntrHandler);

    put_str("Timer_Init end. \n");
}
