file(GLOB CORE_SOURCES "nes/*.c" "mappers/*.c")
idf_component_register(SRCS ${CORE_SOURCES} "nofrendo.c" INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -O3 -Wno-error=format -Wno-format -Wno-error=unused-variable -Wno-error=unused-function)
