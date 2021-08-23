#include <console.h>
#include <stdint.h>

struct position {
    /* 像素参数 */
    uint32_t x_pixel;
    uint32_t y_pixel;
    /* 单个字符像素 */
    uint8_t x_charsize;
    uint8_t y_charsize;
    /* 光标位置 */
    uint32_t x_pos;
    uint32_t y_pos;

    /* 显存起始地址 */
    uint32_t *fb_addr;
    /* 显存大小 */
    uint64_t fb_len;
};

static struct position _8x16_position = {
    .x_pixel = 1440,
    .y_pixel = 900,
    .x_charsize = 8,
    .y_charsize = 16,
    .x_pos = 0,
    .y_pos = 0,
    .fb_addr = (uint32_t *)0xffff800000a00000,
    .fb_len = (1440 * 900 * 4),
};

static struct position *cur_pos = &_8x16_position;

void putchar(char c)
{
	for(int32_t i = 0; i < cur_pos->y_charsize; i++) {
		uint32_t *addr = cur_pos->fb_addr + cur_pos->x_pixel * (cur_pos->y_pos * cur_pos->y_charsize + i) + cur_pos->x_pos * cur_pos->x_charsize;
		uint32_t testval = 0x100;
		for(int32_t j = 0; j < cur_pos->x_charsize; j++) {
			testval = testval >> 1;
            *addr = 0x00000000;
			if(get_fontdata(c * cur_pos->y_charsize + i) & testval) {
				*addr = 0x00ffffff;
            }
            addr++;
		}
	}
}

/* 将字符串打印到串口 */
void puts(const char *str)
{
    if (str == NULL) {
        return;
    }

	const char *s = str;
	while (*s != '\0') {
        if (*s == '\n') {
            cur_pos->y_pos++;
            cur_pos->x_pos = 0;
            s++;
            continue;
        }

		putchar(*s);
        s++;
        cur_pos->x_pos++;
        if (cur_pos->x_pos >= cur_pos->x_pixel / cur_pos->x_charsize) {
            cur_pos->y_pos++;
            cur_pos->x_pos = 0;
        }
	}
}