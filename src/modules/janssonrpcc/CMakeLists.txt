file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)
add_library(jansson::jansson ALIAS PkgConfig::JANSSON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(EVENT REQUIRED IMPORTED_TARGET libevent)
add_library(event::event ALIAS PkgConfig::EVENT)

target_link_libraries(${module_name} PRIVATE jansson::jansson event::event)
