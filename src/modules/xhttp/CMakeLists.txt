# Set the source files
file(GLOB xhttp_SRC *.c)

add_library(xhttp SHARED ${xhttp_SRC})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  xhttp PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/xhttp/=")
set_target_properties(xhttp PROPERTIES SUFFIX ".so")
