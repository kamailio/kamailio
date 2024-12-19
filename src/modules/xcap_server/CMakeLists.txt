find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xcap_server SHARED ${SRC_FILES})

target_link_libraries(xcap_server PRIVATE LibXml2::LibXml2)

target_compile_options(
  xcap_server
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/xcap_server/=")
set_target_properties(xcap_server PROPERTIES SUFFIX ".so")
