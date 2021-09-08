#include <memory.h>
#include <stdint.h>
#include <printk.h>

/* BIOS中通过e820中断号读取的内存信息结构体 */
struct e820_meminfo {
    uintptr_t address;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

struct zone {
    uintptr_t start;
    uintptr_t end;
    uint32_t type;
    uint8_t *bits_map;
    uint64_t bits_size;
    uint64_t bits_len;
    struct zone *next_zone;
};

struct zone *mem_zone_list = NULL;

/* 内核结束地址，链接的时候编译器自动生成 */
extern char _end;

static void mem_zone_set_bits_map(struct zone *p_zone, uint64_t bit_index, uint8_t flag)
{
    uint8_t bits = p_zone->bits_map[bit_index / 8];
    uint8_t bit = bits & (1 << (bit_index % 8));
    /* 如果bit已经是1，则发生了异常 */
    p_zone->bits_map[bit_index / 8] = bits | (flag << (bit_index % 8));

    return;
}

/* 从内存区中获取空闲页表的位图下标，没有空闲页表则返回-1 */
static uint64_t get_free_page_bit_index(const struct zone *p_zone)
{
    for (uint64_t i = 0; i < p_zone->bits_len; i++) {
        /* 位图是1，表示页表已经被占用，如果位图是0xff，则表示已经没有空闲页表 */
        if (p_zone->bits_map[i] != 0xff) {
            /* 一个位图占8bit */
            for (uint8_t j = 0; j < 8; j++) {
                if ((p_zone->bits_map[i] & (1 << j)) == 0) {
                    return i * 8 + j;
                }
            }
        }
    }

    return (uint64_t)-1;
} 

/* 申请一片物理页表，大小为2M */
uintptr_t alloc_page(void)
{
    struct zone *p_zone = mem_zone_list;
    while (p_zone) {
        /* 查找zone中一块空闲的页表 */
        uint64_t bit_index = get_free_page_bit_index(p_zone);
        if (bit_index != (uint64_t)-1) {
            /* 将空闲页表设置为已用 */
            mem_zone_set_bits_map(p_zone, bit_index, 1);
            /* 清空页表并返回对应物理地址 */
            uintptr_t addr = PAGE_2M_ALIGN_UP(p_zone->start + bit_index * PAGE_2M_SIZE);
            memset(addr, 0, PAGE_2M_SIZE);
            return addr;
        }
        p_zone = p_zone->next_zone;
    }

    /* 申请失败，返回-1 */
    return NULL;
}

/* 释放一片物理页表，大小为2M */
void free_page(uintptr_t addr)
{
    struct zone *p_zone = mem_zone_list;
    while (p_zone) {
        if ((p_zone->start <= addr) && (p_zone->end >= addr)) {
            mem_zone_set_bits_map(p_zone, PAGE_2M_ALIGN_DOWN(addr - p_zone->start) / PAGE_2M_SIZE, 0);
        }
    }

    return;
}

void init_mem(void)
{
    printk("start to init mem...\n");
    /* 在BIOS中将内存信息存放在0x7e00开始处 */
    struct e820_meminfo *p = (struct e820_meminfo *)0xffff800000007e00;
    uint64_t total_mem = 0;
    uintptr_t zone_start_addr = (uintptr_t)&_end;
    for (uint8_t i = 0; i < 32; i++) {
        printk("Address:%ld  Length:%ld  Type:%d\n", p->address, p->len, p->type);
        if (p->type != 1) {
            continue;
        }

        uintptr_t start = PAGE_2M_ALIGN_UP(p->address);
        uintptr_t end = PAGE_2M_ALIGN_DOWN(p->address + p->len);
        if (end <= start) {
            continue;
        }

        /* zone列表存放在内核数据后面，格式如下： 
         * ---------------------------------------------------------------
         * |     kernel     | zone1 | bits_map1 | zone2 | bit_map2 | ... |
         * ---------------------------------------------------------------
         * _text            _end                
        */
        struct zone *p_zone = (struct zone *)zone_start_addr;
        p_zone->start = start;
        p_zone->end = end;
        p_zone->type = p->type;
        /* zone的bits_map地址 */
        p_zone->bits_map = (uint8_t *)(zone_start_addr + sizeof(struct zone));
        p_zone->bits_size = (end - start) / PAGE_2M_SIZE;
        p_zone->bits_len = (p_zone->bits_size + 8 - 1) / 8;
        /* 初始化bits_map为全0，表示所有页表未被使用 */
        memset(p_zone->bits_map, 0, p_zone->bits_len);
        p_zone->next_zone = NULL;

        /* 将zone存放至列表中 */
        if (mem_zone_list == NULL) {
            mem_zone_list = p_zone;
            mem_zone_list->next_zone = NULL;
        } else {
            struct zone *last_zone = mem_zone_list;
            while (last_zone->next_zone) {
                last_zone = last_zone->next_zone;
            }
            last_zone->next_zone = p_zone;
        }

        /* 下一个zone的起始地址 */
        zone_start_addr = (uint8_t *)zone_start_addr + sizeof(struct zone) + p_zone->bits_len;
        p++;
    }

    /* zone_start_addr已经被使用的物理内存地址，需要将使用标志置上 */
    struct zone *p_zone = mem_zone_list;
    while (p_zone) {
        if ((p_zone->start > zone_start_addr) && (p_zone->end >= zone_start_addr)) {
            for (uint32_t i = 0; i <= (zone_start_addr - p_zone->start - 1) / PAGE_2M_SIZE; i++) {
                mem_zone_set_bits_map(p_zone, i, 1);
            }
        }
        p_zone = p_zone->next_zone;
    }

    printk("finish to init mem...\n");
    
    return;
}