file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SECSIPID2 QUIET IMPORTED_TARGET secsipid-2)
if(SECSIPID2_FOUND)
  add_library(secsipid ALIAS PkgConfig::SECSIPID2)
else()
  pkg_check_modules(SECSIPID1 REQUIRED IMPORTED_TARGET secsipid-1)
  add_library(secsipid ALIAS PkgConfig::SECSIPID1)
endif()

if(APPLE)
  target_link_libraries(${module_name} PRIVATE "-framework CoreFoundation" "-framework Security")
endif()

target_link_libraries(${module_name} PRIVATE secsipid Threads::Threads)
