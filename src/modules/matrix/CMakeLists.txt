file(GLOB SRC_FILES "*.c")

add_library(matrix SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  matrix PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/matrix/=")
set_target_properties(matrix PROPERTIES SUFFIX ".so")
