/*
 *  kernel/device/timer.h
 *
 *  (C) 2021  Jacky
 */
#ifndef TIMER_H
#define TIMER_H

#include "stdint.h"

/* 以毫秒为单位sleep */
void Timer_SleepMTime(uint32_t mSeconds);

void Timer_Init(void);

#endif