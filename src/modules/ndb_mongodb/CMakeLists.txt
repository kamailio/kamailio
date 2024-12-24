file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(mongoc REQUIRED IMPORTED_TARGET libmongoc-1.0>=1.7)
add_library(mongoc::mongoc ALIAS PkgConfig::mongoc)

target_link_libraries(${module_name} PRIVATE mongoc::mongoc)
