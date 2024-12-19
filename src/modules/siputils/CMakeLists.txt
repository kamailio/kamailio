file(GLOB SRC_FILES "*.c")

add_library(siputils SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  siputils
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/siputils/=")
set_target_properties(siputils PROPERTIES SUFFIX ".so")
