#!/bin/bash
set -e

ROOT_DIR=$(pwd)/../
OUTPUT_DIR="${ROOT_DIR}"/output

rm -rf "${ROOT_DIR}"/output
mkdir -p "${ROOT_DIR}"/output

# 编译
rm -rf hp_kernel_build
mkdir -p hp_kernel_build
cd hp_kernel_build
cmake ${ROOT_DIR}
make

# 制作镜像
rm -rf ${OUTPUT_DIR}/Hp-Kernel.bin
dd if=/dev/zero of=${OUTPUT_DIR}/Hp-Kernel.bin bs=512 count=10240
dd if=${OUTPUT_DIR}/mbr.bin of=${OUTPUT_DIR}/Hp-Kernel.bin bs=512 conv=notrunc
dd if=${OUTPUT_DIR}/loader.bin of=${OUTPUT_DIR}/Hp-Kernel.bin bs=512 seek=2 conv=notrunc
dd if=${OUTPUT_DIR}/kernel.bin of=${OUTPUT_DIR}/Hp-Kernel.bin bs=512 seek=9 conv=notrunc

# 清除临时产物
rm -rf ${OUTPUT_DIR}/mbr.bin
rm -rf ${OUTPUT_DIR}/loader.bin
rm -rf ${OUTPUT_DIR}/kernel.bin

exit 0