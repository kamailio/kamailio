file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Libev REQUIRED)

target_link_libraries(${module_name} PRIVATE Libev::Libev)
