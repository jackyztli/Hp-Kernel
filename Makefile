SHELL=/bin/bash
CFLAGS = -march=i686 -fno-builtin -fno-PIC -Wall -ggdb -m32 -gstabs -nostdinc -fno-stack-protector -Os -nostdinc
LDFLAGS = -m elf_i386 -nostdlib -N

CC = gcc
LD = ld
OBJCOPY = objcopy
DD = dd

.c.s:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -S -o $*.s $<

.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $*.o $<

all: Image

Image: boot
	mkdir -p output
	$(DD) if=/dev/zero of=output/Image bs=512 count=10
	$(DD) if=boot/boot of=output/Image bs=512 count=1 conv=notrunc

boot: boot/boot.o boot/loader.o boot/tmp.out
	$(LD) $(LDFLAGS) -e start -Ttext 0x7C00 boot/boot.o boot/loader.o -o boot/boot.out
	$(OBJCOPY) -S -O binary boot/boot.out boot/boot.out
	$(DD) if=/dev/zero of=boot/boot bs=512 count=1
	$(DD) if=boot/boot.out of=boot/boot bs=512 count=1 conv=notrunc
	$(DD) if=boot/tmp.out of=boot/boot bs=510 seek=1 conv=notrunc

boot/boot.o: boot/boot.s
	$(CC) $(CFLAGS) -c boot/boot.s -o boot/boot.o 

boot/loader.o: boot/loader.c

boot/tmp.out:
	(echo -e -n "\x55\xaa") >boot/tmp.out

clean:
	rm -rf boot/*.o boot/*.out boot/boot
	rm -rf output