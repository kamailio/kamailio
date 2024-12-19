find_package(PkgConfig REQUIRED)
pkg_check_modules(libruxc REQUIRED IMPORTED_TARGET libruxc)
add_library(libruxc::libruxc ALIAS PkgConfig::libruxc)

file(GLOB SRC_FILES "*.c")

add_library(ruxc SHARED ${SRC_FILES})

if(APPLE)
  target_link_libraries(ruxc PRIVATE "-framework CoreFoundation"
                                     "-framework Security")
endif()
target_link_libraries(ruxc PRIVATE libruxc::libruxc)

target_compile_options(
  ruxc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ruxc/=")
set_target_properties(ruxc PROPERTIES SUFFIX ".so")
