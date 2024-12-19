file(GLOB SRC_FILES "*.c")

add_library(rtpproxy SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  rtpproxy
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/rtpproxy/=")
set_target_properties(rtpproxy PROPERTIES SUFFIX ".so")
