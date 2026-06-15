file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# tps_common.c (symlink to ../topos) includes topos headers
target_include_directories(${module_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../topos")
