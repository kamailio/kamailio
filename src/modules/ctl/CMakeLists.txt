file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# TODO: Should this be picked by user defined or hardcoded?
target_compile_definitions(${module_name} PRIVATE USE_FIFO CTL_SYSTEM_MALLOC)
