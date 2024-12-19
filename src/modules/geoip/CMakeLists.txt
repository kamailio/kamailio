file(GLOB SRC_FILES "*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(geoip REQUIRED IMPORTED_TARGET geoip)

add_library(geoip SHARED ${SRC_FILES})
target_include_directories(geoip PRIVATE ${geoip_INCLUDE_DIRS})
target_link_libraries(geoip PRIVATE ${geoip_LIBRARIES})

target_compile_options(
  geoip PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/geoip/=")
set_target_properties(geoip PROPERTIES SUFFIX ".so")
