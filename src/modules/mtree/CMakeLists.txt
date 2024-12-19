file(GLOB SRC_FILES "*.c")

add_library(mtree SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  mtree PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/mtree/=")
set_target_properties(mtree PROPERTIES SUFFIX ".so")
