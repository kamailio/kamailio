file(GLOB SRC_FILES "*.c")

add_library(permissions SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  permissions
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/permissions/=")
set_target_properties(permissions PROPERTIES SUFFIX ".so")
