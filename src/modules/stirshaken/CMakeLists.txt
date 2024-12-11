find_package(PkgConfig REQUIRED)
pkg_check_modules(stirshaken REQUIRED IMPORTED_TARGET libstireshaken)
add_library(stirshaken::stirshaken ALIAS PkgConfig::stirshaken)

file(GLOB SRC_FILES "*.c")

add_library(stirshaken SHARED ${SRC_FILES})
target_link_libraries(stirshaken PRIVATE stirshaken::stirshaken)

target_compile_options(stirshaken PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/stirshaken/=")
