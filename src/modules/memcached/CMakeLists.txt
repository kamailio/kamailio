find_package(PkgConfig REQUIRED)
pkg_check_modules(memcached REQUIRED IMPORTED_TARGET libmemcached)
add_library(memcached::memcached ALIAS PkgConfig::memcached)

file(GLOB SRC_FILES "*.c")

add_library(memcached SHARED ${SRC_FILES})
target_link_libraries(memcached PRIVATE memcached::memcached)

target_compile_options(memcached PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/memcached/=")
