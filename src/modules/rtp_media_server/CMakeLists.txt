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

file(GLOB SRC_FILES "*.c")

add_library(rtp_media_server SHARED ${SRC_FILES})

target_link_libraries(rtp_media_server PRIVATE ortp mediastreamer2)
# target_link_libraries(.*PRIVATE)
