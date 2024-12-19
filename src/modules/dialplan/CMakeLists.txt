file(GLOB SRC_FILES "*.c")

add_library(dialplan SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  dialplan
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/dialplan/=")
set_target_properties(dialplan PROPERTIES SUFFIX ".so")
