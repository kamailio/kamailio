file(GLOB SRC_FILES "*.c")

add_library(call_control SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  call_control
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/call_control/=")
set_target_properties(call_control PROPERTIES SUFFIX ".so")
