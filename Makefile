SHELL=/bin/bash
CFLAGS_32 = -march=i386 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc -fno-stack-protector -Os -nostdinc
LDFLAGS_32 = -m elf_i386 -nostdlib -N

CFLAGS_64 = -fno-builtin -fno-PIC -Wall -ggdb -gstabs -nostdinc -fno-stack-protector -Os -nostdinc
LDFLAGS_64 = -nostdlib -N

CC = gcc
LD = ld
OBJCOPY = objcopy
DD = dd

.c.s:
	$(CC) $(CFLAGS_64) \
	-nostdinc -Iinclude -S -o $*.s $<

.c.o:
	$(CC) $(CFLAGS_64) \
	-nostdinc -Iinclude -c -o $*.o $<

all: Image

Image: boot kernel
	mkdir -p output
	$(DD) if=/dev/zero of=output/Image bs=512 count=10
	$(DD) if=output/boot of=output/Image bs=512 count=1 conv=notrunc
	$(DD) if=output/kernel of=output/Image bs=512 count=7 seek=2 conv=notrunc

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

# 内核部分
kernel: init/head.o init/main.o
	mkdir -p output
	$(LD) $(LDFLAGS_64) -e setup -Ttext 0x100000 init/head.o init/main.o -o output/kernel
	$(OBJCOPY) -S -O binary output/kernel output/kernel

clean:
	rm -rf boot/*.o boot/*.out boot/boot
	rm -rf init/*.o
	rm -rf output