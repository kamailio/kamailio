file(GLOB SRC_FILES "*.c")

add_library(textops SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  textops PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/textops/=")
set_target_properties(textops PROPERTIES SUFFIX ".so")
