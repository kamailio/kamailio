find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
add_library(uuid::uuid ALIAS PkgConfig::UUID)

file(GLOB SRC_FILES "*.c")

add_library(uuid SHARED ${SRC_FILES})
target_link_libraries(uuid PRIVATE uuid::uuid)

target_compile_options(uuid PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uuid/=")
