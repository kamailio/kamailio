file(GLOB SRC_FILES "*.c")

add_library(secfilter SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  secfilter
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/secfilter/=")
set_target_properties(secfilter PROPERTIES SUFFIX ".so")
