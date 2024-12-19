file(GLOB SRC_FILES "*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(geoip REQUIRED IMPORTED_TARGET geoip)

add_library(geoip SHARED ${SRC_FILES})
target_include_directories(geoip PRIVATE ${geoip_INCLUDE_DIRS})
target_link_libraries(geoip PRIVATE ${geoip_LIBRARIES})
