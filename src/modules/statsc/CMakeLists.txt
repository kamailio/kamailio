file(GLOB SRC_FILES "*.c")

add_library(statsc SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  statsc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/statsc/=")
set_target_properties(statsc PROPERTIES SUFFIX ".so")
