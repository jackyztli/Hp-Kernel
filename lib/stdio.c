/*
 *  lib/stdio.c
 *
 *  (C) 2021  Jacky
 */

#include "stdio.h"
#include "lib/string.h"

#define va_start(ap, v) ap = (va_list)&v  // 把ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))	  // ap指向下一个参数并返回其值
#define va_end(ap) ap = NULL		  // 清除ap

/* 将整型转换成字符(integer to ascii) */
static void itoa(uint32_t value, char **buf_ptr_addr, uint8_t base)
{
    uint32_t m = value % base;	    // 求模,最先掉下来的是最低位   
    uint32_t i = value / base;	    // 取整
    if (i) {			    // 如果倍数不为0则递归调用。
        itoa(i, buf_ptr_addr, base);
    }
    if (m < 10) {      // 如果余数是0~9
        *((*buf_ptr_addr)++) = m + '0';	  // 将数字0~9转换为字符'0'~'9'
    } else {	      // 否则余数是A~F
        *((*buf_ptr_addr)++) = m - 10 + 'A'; // 将数字A~F转换为字符'A'~'F'
    }

    return;
}

uint32_t vsprintf(char *str, const char *format, va_list ap)
{
    char *buf = str;
    const char *index = format;
    char c = *index;
    int32_t argInt;
    char *argStr = NULL;
    while(c) {
        if (c != '%') {
	        *(buf++) = c;
	        c = *(++index);
	        continue;
        }
        c = *(++index);	 // 得到%后面的字符
        switch(c) {
	        case 's':
	            argStr = va_arg(ap, char*);
	            strcpy(buf, argStr);
	            buf += strlen(argStr);
	            c = *(++index);
	            break;

	        case 'c':
	            *(buf++) = va_arg(ap, char);
	            c = *(++index);
	            break;

	        case 'd':
	            argInt = va_arg(ap, int);
        /* 若是负数, 将其转为正数后,再正数前面输出个负号'-'. */
	            if (argInt < 0) {
	                argInt = 0 - argInt;
	                *buf++ = '-';
	            }
	            itoa(argInt, &buf, 10); 
	            c = *(++index);
	            break;

	        case 'x':
	            argInt = va_arg(ap, int);
	            itoa(argInt, &buf, 16); 
	            c = *(++index); // 跳过格式字符并更新index_char
	            break;
        }
    }

    return strlen(str);
}

uint32_t sprintf(char* buf, const char* format, ...)
{
   va_list args;
   va_start(args, format);
   uint32_t retval = vsprintf(buf, format, args);
   va_end(args);

   return retval;
}
