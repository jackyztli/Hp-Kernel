CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

project(Hp-Kernel)

set(CMAKE_VERBOSE_MAKEFILE on)

set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OUTPUT_DIR ${ROOT_DIR}/output)

# 设置编译工具
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_C_FLAGS "-m32 -fno-builtin")
set(CMAKE_ASM_COMPILER "nasm")

add_subdirectory(mbr)
add_subdirectory(lib)
add_subdirectory(kernel)