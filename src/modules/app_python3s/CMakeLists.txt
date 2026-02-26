file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Python3 REQUIRED COMPONENTS Development)

target_link_libraries(${module_name} PRIVATE Python3::Python)
