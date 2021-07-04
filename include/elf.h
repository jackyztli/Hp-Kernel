#ifndef __ELF_H__
#define __ELF_H__

#include <stdint.h>

#define EI_NIDENT 16
#define ELF_MAGIC    0x464C457FU

/* elf格式头定义 */
typedef struct
{
    uint8_t   ident[EI_NIDENT];     /* 魔数字 */
    uint16_t  type;                 /* 目标文件类型 */
    uint16_t  machine;              /* 目标文件体系结构类型 */
    uint32_t  version;              /* 目标文件版本 */
    uint64_t  entry;                /* 目标文件程序虚拟入口 */
    uint64_t  phoff;                /* 目标文件段表偏移量 */
    uint64_t  shoff;                /* 目标文件节表偏移量 */
    uint32_t  flags;                /* 处理器相关标识 */
    uint16_t  ehsize;               /* ELF头大小 */
    uint16_t  phentsize;            /* 段实体大小 */
    uint16_t  phnum;                /* 段实体数量 */
    uint16_t  shentsize;            /* 节点实体大小 */
    uint16_t  shnum;                /* 节点实体数量 */
    uint16_t  shstrndx;             /* 字符串表下标 */
} elfhdr;

typedef struct
{
  uint32_t    type;                 /* 段类型 */
  uint32_t    flags;                /* 段标识 */
  uint64_t    offset;               /* 段相对于efl头偏移 */
  uint64_t    vaddr;                /* 段虚拟地址 */
  uint64_t    paddr;                /* 段物理地址 */
  uint64_t    filesz;               /* 段在文件中大小 */
  uint64_t    memsz;                /* 段在加载到内存中大小 */
  uint64_t    align;                /* 段对齐 */
} elfphdr;

#endif