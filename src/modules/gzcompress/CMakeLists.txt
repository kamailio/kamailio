file(GLOB SRC_FILES "*.c")

find_package(PkgConfig REQUIRED)
pkg_check_modules(zlib REQUIRED IMPORTED_TARGET zlib)
add_library(zlib::zlib ALIAS PkgConfig::zlib)

add_library(gzcompress SHARED ${SRC_FILES})
target_link_libraries(gzcompress PRIVATE zlib::zlib)
