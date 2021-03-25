add_custom_command(
    OUTPUT ${OUTPUT_DIR}/mbr.bin
    COMMAND ${CMAKE_ASM_COMPILER} -o ${OUTPUT_DIR}/mbr.bin mbr.s
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(
    OUTPUT ${OUTPUT_DIR}/loader.bin
    COMMAND ${CMAKE_ASM_COMPILER} -o ${OUTPUT_DIR}/loader.bin loader.s
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(mbr_loader_src ALL
    DEPENDS ${OUTPUT_DIR}/mbr.bin
            ${OUTPUT_DIR}/loader.bin)