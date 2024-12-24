file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(libruxc REQUIRED IMPORTED_TARGET libruxc)
add_library(libruxc::libruxc ALIAS PkgConfig::libruxc)

if(APPLE)
  target_link_libraries(${module_name} PRIVATE "-framework CoreFoundation"
                                               "-framework Security")
endif()
target_link_libraries(${module_name} PRIVATE libruxc::libruxc)
