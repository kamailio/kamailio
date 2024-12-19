file(GLOB SRC_FILES "*.c")

add_library(pv_headers SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  pv_headers
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pv_headers/=")
set_target_properties(pv_headers PROPERTIES SUFFIX ".so")
