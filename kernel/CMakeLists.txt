set(KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB_DIR ${ROOT_DIR}/lib/)
set(FS_DIR ${ROOT_DIR}/fs/)

add_custom_command(
    OUTPUT kernel.o
    COMMAND ${CMAKE_ASM_COMPILER} -f elf -o kernel.o kernel.s
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(
    OUTPUT print.o
    COMMAND ${CMAKE_ASM_COMPILER} -f elf -o print.o ${ROOT_DIR}/lib/print.s
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(
    OUTPUT switch.o
    COMMAND ${CMAKE_ASM_COMPILER} -f elf -o switch.o switch.s
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

set(KERNEL_SRC main.c kernel.o syscall.c console.c process.c tss.c sync.c thread.c memory.c bitmap.c ${LIB_DIR}/list.c ${LIB_DIR}/string.c ${LIB_DIR}/stdio.c panic.c interrupt.c device/ide.c device/timer.c print.o switch.o ${FS_DIR}/inode.c ${FS_DIR}/fs.c ${FS_DIR}/dir.c ${FS_DIR}/file.c)
set(KERNEL_O main.o kernel.o syscall.o console.o process.o tss.o sync.o thread.o memory.o bitmap.o list.o string.o stdio.o panic.o interrupt.o ide.o timer.o print.o switch.o inode.o fs.o dir.o file.o)
add_custom_command(
    OUTPUT kernel.bin
    COMMAND ${CMAKE_C_COMPILER} -c ${KERNEL_SRC} -I${ROOT_DIR}/include/ -I${ROOT_DIR}/ -fno-stack-protector -fno-builtin -m32 -nostartfiles
    COMMAND ${CMAKE_LD_LINKER} -o kernel.bin -melf_i386 -Ttext 0xc0001500 -e main -Map kernel.map ${KERNEL_O}
    COMMAND mv -f kernel.bin ${OUTPUT_DIR}
    COMMAND mv -f kernel.map ${OUTPUT_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(kernel_bin_src ALL
    DEPENDS kernel.o
            print.o
            switch.o
            kernel.bin)