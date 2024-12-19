file(GLOB SRC_FILES "*.c")

add_library(stun SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  stun PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/stun/=")
set_target_properties(stun PROPERTIES SUFFIX ".so")
