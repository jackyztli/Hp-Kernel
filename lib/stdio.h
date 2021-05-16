/*
 *  lib/stdio.h
 *
 *  (C) 2021  Jacky
 */
#ifndef STDIO_H
#define STDIO_H

#include "stdint.h"

typedef char* va_list;
uint32_t vsprintf(char *str, const char *format, va_list ap);
uint32_t sprintf(char *buf, const char *format, ...);

#endif