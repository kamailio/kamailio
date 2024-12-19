find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_bla SHARED ${SRC_FILES})
target_link_libraries(pua_bla PRIVATE LibXml2::LibXml2)

target_compile_options(
  pua_bla PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua_bla/=")
set_target_properties(pua_bla PROPERTIES SUFFIX ".so")
