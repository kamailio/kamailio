file(GLOB SRC_FILES "*.c")

add_library(xlog SHARED ${SRC_FILES})
# # target_link_libraries(.*PRIVATE)

target_compile_options(
  xlog PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/xlog/=")
set_target_properties(xlog PROPERTIES SUFFIX ".so")
