file(GLOB SRC_FILES "*.c")

add_library(textopsx SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  textopsx
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/textopsx/=")
set_target_properties(textopsx PROPERTIES SUFFIX ".so")
