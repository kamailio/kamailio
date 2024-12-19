file(GLOB SRC_FILES "*.c")

add_library(cfgutils SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  cfgutils
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/cfgutils/=")
set_target_properties(cfgutils PROPERTIES SUFFIX ".so")
