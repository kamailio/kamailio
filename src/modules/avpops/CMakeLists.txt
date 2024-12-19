file(GLOB SRC_FILES "*.c")

add_library(avpops SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  avpops PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/avpops/=")
set_target_properties(avpops PROPERTIES SUFFIX ".so")
