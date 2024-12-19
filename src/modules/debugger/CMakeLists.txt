file(GLOB SRC_FILES "*.c")

add_library(debugger SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  debugger
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/debugger/=")
set_target_properties(debugger PROPERTIES SUFFIX ".so")
