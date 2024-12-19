find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_dialoginfo SHARED ${SRC_FILES})
target_link_libraries(pua_dialoginfo PRIVATE LibXml2::LibXml2)

target_compile_options(
  pua_dialoginfo
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua_dialoginfo/=")
set_target_properties(pua_dialoginfo PROPERTIES SUFFIX ".so")
