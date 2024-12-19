find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)
add_library(jansson::jansson ALIAS PkgConfig::JANSSON)

file(GLOB SRC_FILES "*.c")

add_library(acc_json SHARED ${SRC_FILES})
target_link_libraries(acc_json PRIVATE jansson::jansson)

target_compile_options(acc_json PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/acc_json/=")
