file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)

# ortp and mediastreamer2 do have cmake support on newer version but
# is not up to standard and complicates things.
# So we use pkg-config to find them.
pkg_check_modules(ortp REQUIRED IMPORTED_TARGET ortp)
add_library(ortp ALIAS PkgConfig::ortp)

pkg_check_modules(mediastreamer2 REQUIRED IMPORTED_TARGET mediastreamer)
add_library(mediastreamer2 ALIAS PkgConfig::mediastreamer2)

target_link_libraries(${module_name} PRIVATE ortp mediastreamer2)
