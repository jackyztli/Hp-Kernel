SHELL=/bin/bash
CFLAGS_32 = -march=i386 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc -fno-stack-protector -Os -nostdinc
LDFLAGS_32 = -m elf_i386 -nostdlib -N

CFLAGS_64 = -fno-builtin -mcmodel=large -Wall -ggdb -nostdinc -fno-stack-protector -m64
LDFLAGS_64 = -b elf64-x86-64 -z muldefs -nostdlib -N

CC = gcc
LD = ld
OBJCOPY = objcopy
DD = dd

KERNEL_INCLUDE = -I./include \
				 -I./kernel/ \
				 -I./mm/     \
				 -I./lib/	 \
				 -I./kernel/drivers/console/ \
				 -I./kernel/drivers/8259A/

.c.s:
	$(CC) $(CFLAGS_64) \
	$(KERNEL_INCLUDE) -S -o $*.s $<

.c.o:
	$(CC) $(CFLAGS_64) \
	$(KERNEL_INCLUDE) -c -o $*.o $<

all: Image

Image: boot setup kernel
	mkdir -p output
	$(DD) if=/dev/zero of=output/Image bs=512 count=102
	$(DD) if=output/boot of=output/Image bs=512 count=1 conv=notrunc
	$(DD) if=output/setup of=output/Image bs=512 count=4 seek=2 conv=notrunc
	$(DD) if=output/kernel of=output/Image bs=512 count=90 seek=6 conv=notrunc

# boot部分
boot: boot/boot.o boot/loader.o boot/tmp.out
	$(LD) $(LDFLAGS_32) -e start -Ttext 0x7C00 boot/boot.o boot/loader.o -o boot/boot.out
	$(OBJCOPY) -S -O binary boot/boot.out boot/boot.out
	$(DD) if=/dev/zero of=boot/boot bs=512 count=1
	$(DD) if=boot/boot.out of=boot/boot bs=512 count=1 conv=notrunc
	$(DD) if=boot/tmp.out of=boot/boot bs=510 seek=1 conv=notrunc
	mkdir -p output && cp -rf boot/boot output/

boot/boot.o: boot/boot.s
	$(CC) $(CFLAGS_32) -c boot/boot.s -o boot/boot.o 

boot/loader.o: boot/loader.c
	$(CC) $(CFLAGS_32) -nostdinc -Iinclude -c boot/loader.c -o boot/loader.o 

boot/tmp.out:
	(echo -e -n "\x55\xaa") >boot/tmp.out

setup: boot/setup.s
	$(CC) $(CFLAGS_32) -c boot/setup.s -o boot/setup.o
	$(LD) $(LDFLAGS_32) -e setup -Ttext 0x80000 boot/setup.o -o boot/setup
	mkdir -p output
	$(OBJCOPY) -S -O binary boot/setup output/setup

KERNEL_OBJ = init/head.o init/main.o kernel/printk.o kernel/drivers/console/console.o kernel/drivers/console/font_8x16.o kernel/trap.o \
			 kernel/trap_entry.o mm/memory.o lib/string.o kernel/drivers/8259A/interrupt.o 
# 内核部分
kernel: $(KERNEL_OBJ)
	mkdir -p output
	$(LD) $(LDFLAGS_64) -e setup64 -Ttext 0xffff800000100000 $(KERNEL_OBJ) -o output/kernel

clean:
	rm -rf boot/*.o boot/*.out boot/boot
	rm -rf init/*.o
	rm -rf kernel/*.o
	rm -rf kernel/drivers/console/*.o
	rm -rf kernel/drivers/8259A/*.o
	rm -rf lib/*.o
	rm -rf output