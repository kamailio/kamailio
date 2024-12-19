find_package(PkgConfig REQUIRED)
pkg_check_modules(libruxc REQUIRED IMPORTED_TARGET libruxc)
add_library(libruxc::libruxc ALIAS PkgConfig::libruxc)

file(GLOB SRC_FILES "*.c")

add_library(ruxc SHARED ${SRC_FILES})

if(APPLE)
  target_link_libraries(ruxc PRIVATE "-framework CoreFoundation"
                                     "-framework Security")
endif()
target_link_libraries(ruxc PRIVATE libruxc::libruxc)
