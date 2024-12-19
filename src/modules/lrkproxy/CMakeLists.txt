file(GLOB SRC_FILES "*.c")

add_library(lrkproxy SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  lrkproxy
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/lrkproxy/=")
set_target_properties(lrkproxy PROPERTIES SUFFIX ".so")
