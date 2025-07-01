file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# python2-dev package is required for Python2::Module target
find_package(Python2 REQUIRED COMPONENTS Development)

target_link_libraries(${module_name} PRIVATE Python2::Module)
