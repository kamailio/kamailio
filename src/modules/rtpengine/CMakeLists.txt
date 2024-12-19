file(GLOB SRC_FILES "*.c")

add_library(rtpengine SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  rtpengine
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/rtpengine/=")
set_target_properties(rtpengine PROPERTIES SUFFIX ".so")
