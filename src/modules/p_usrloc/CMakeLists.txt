file(GLOB SRC_FILES "*.c")

add_library(p_usrloc SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  p_usrloc
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/p_usrloc/=")
set_target_properties(p_usrloc PROPERTIES SUFFIX ".so")
