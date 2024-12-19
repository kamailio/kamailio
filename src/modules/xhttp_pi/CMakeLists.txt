find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xhttp_pi SHARED ${SRC_FILES})

target_link_libraries(xhttp_pi PRIVATE LibXml2::LibXml2)

target_compile_options(
  xhttp_pi
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/xhttp_pi/=")
set_target_properties(xhttp_pi PROPERTIES SUFFIX ".so")
