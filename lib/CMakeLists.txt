add_custom_command(
    OUTPUT print.o
    COMMAND ${CMAKE_ASM_COMPILER} -f elf -o print.o print.s
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
