# libmongoc-1.0 is required for this module Missing dependency on Ubuntu 20.04
# of libzstd-dev find_package(zstd REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(mongoc REQUIRED IMPORTED_TARGET libmongoc-1.0>=1.7)
add_library(mongoc::mongoc ALIAS PkgConfig::mongoc)

file(GLOB SRC_FILES "*.c")

add_library(db_mongodb SHARED ${SRC_FILES})

target_link_libraries(db_mongodb PRIVATE mongoc::mongoc)
