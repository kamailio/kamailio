file(GLOB SRC_FILES "*.c")

add_library(userblocklist SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  userblocklist
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/userblocklist/=")
set_target_properties(userblocklist PROPERTIES SUFFIX ".so")
