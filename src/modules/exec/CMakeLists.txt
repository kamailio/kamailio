file(GLOB SRC_FILES "*.c")

add_library(exec SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  exec PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/exec/=")
set_target_properties(exec PROPERTIES SUFFIX ".so")
