file(GLOB SRC_FILES "*.c")

add_library(blst SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  blst PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/blst/=")
set_target_properties(blst PROPERTIES SUFFIX ".so")
