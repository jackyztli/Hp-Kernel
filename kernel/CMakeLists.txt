set(KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(LIB_DIR ${ROOT_DIR}/lib/)

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

set(KERNEL_SRC main.c kernel.o console.c sync.c thread.c memory.c bitmap.c ${LIB_DIR}/list.c ${LIB_DIR}/string.c panic.c interrupt.c device/timer.c print.o switch.o)
add_custom_command(
    OUTPUT kernel.bin
    COMMAND ${CMAKE_C_COMPILER} -o kernel.bin ${KERNEL_SRC} -I${ROOT_DIR}/include/ -I${ROOT_DIR}/ -fno-builtin -m32 -Ttext 0xc0001500 -e main
    COMMAND mv -f kernel.bin ${OUTPUT_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(kernel_bin_src ALL
    DEPENDS kernel.o
            print.o
            switch.o
            kernel.bin)