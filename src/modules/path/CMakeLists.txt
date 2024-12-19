file(GLOB SRC_FILES "*.c")

add_library(path SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  path PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/path/=")
set_target_properties(path PROPERTIES SUFFIX ".so")
