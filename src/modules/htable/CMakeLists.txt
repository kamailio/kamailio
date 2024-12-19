file(GLOB SRC_FILES "*.c")

add_library(htable SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  htable PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/htable/=")
set_target_properties(htable PROPERTIES SUFFIX ".so")
