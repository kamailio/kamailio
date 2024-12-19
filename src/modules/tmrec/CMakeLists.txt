file(GLOB SRC_FILES "*.c")

add_library(tmrec SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  tmrec PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/tmrec/=")
set_target_properties(tmrec PROPERTIES SUFFIX ".so")
