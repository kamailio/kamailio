file(GLOB SRC_FILES "*.c")

add_library(evrexec SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  evrexec PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/evrexec/=")
set_target_properties(evrexec PROPERTIES SUFFIX ".so")
