file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(ortp QUIET)
if(NOT ortp_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(ortp REQUIRED IMPORTED_TARGET ortp)
  add_library(ortp ALIAS PkgConfig::ortp)
endif()

find_package(mediastreamer2 QUIET)
if(NOT mediastreamer2_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(mediastreamer2 REQUIRED IMPORTED_TARGET mediastreamer)
  add_library(mediastreamer2 ALIAS PkgConfig::mediastreamer2)
endif()

target_link_libraries(${module_name} PRIVATE ortp mediastreamer2)
