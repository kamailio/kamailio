file(GLOB SRC_FILES "*.c")

add_library(mediaproxy SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  mediaproxy
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/mediaproxy/=")
set_target_properties(mediaproxy PROPERTIES SUFFIX ".so")
