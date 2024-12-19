file(GLOB SRC_FILES "*.c")

add_library(h350 SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  h350 PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/h350/=")
set_target_properties(h350 PROPERTIES SUFFIX ".so")
