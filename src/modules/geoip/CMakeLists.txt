file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(geoip REQUIRED IMPORTED_TARGET geoip)

target_include_directories(${module_name} PRIVATE ${geoip_INCLUDE_DIRS})
target_link_libraries(${module_name} PRIVATE ${geoip_LIBRARIES})
