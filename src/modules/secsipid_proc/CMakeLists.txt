file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(secsipid-1 REQUIRED IMPORTED_TARGET secsipid-1)
add_library(secsipid ALIAS PkgConfig::secsipid-1)

if(APPLE)
  target_link_libraries(${module_name} PRIVATE "-framework CoreFoundation" "-framework Security")
endif()

target_link_libraries(${module_name} PRIVATE secsipid Threads::Threads)
