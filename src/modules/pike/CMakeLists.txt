file(GLOB SRC_FILES "*.c")

add_library(pike SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  pike PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pike/=")
set_target_properties(pike PROPERTIES SUFFIX ".so")
