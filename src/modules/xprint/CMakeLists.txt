file(GLOB SRC_FILES "*.c")

add_library(xprint SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  xprint PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/xprint/=")
set_target_properties(xprint PROPERTIES SUFFIX ".so")
