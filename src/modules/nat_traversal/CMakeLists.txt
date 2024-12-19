file(GLOB SRC_FILES "*.c")

add_library(nat_traversal SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  nat_traversal
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/nat_traversal/=")
set_target_properties(nat_traversal PROPERTIES SUFFIX ".so")
