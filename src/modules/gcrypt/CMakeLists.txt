file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(gcrypt REQUIRED IMPORTED_TARGET libgcrypt)
add_library(gcrypt::gcrypt ALIAS PkgConfig::gcrypt)

target_link_libraries(${module_name} PRIVATE gcrypt::gcrypt)
