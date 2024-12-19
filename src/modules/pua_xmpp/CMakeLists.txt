find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_xmpp SHARED ${SRC_FILES})
target_link_libraries(pua_xmpp PRIVATE LibXml2::LibXml2)

target_compile_options(
  pua_xmpp
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua_xmpp/=")
set_target_properties(pua_xmpp PROPERTIES SUFFIX ".so")
