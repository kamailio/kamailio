file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(thrift REQUIRED IMPORTED_TARGET thrift)
add_library(thrift::thrift ALIAS PkgConfig::thrift)

target_link_libraries(${module_name} PRIVATE thrift::thrift)
