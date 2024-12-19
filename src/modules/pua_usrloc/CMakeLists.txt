find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua_usrloc SHARED ${SRC_FILES})
target_link_libraries(pua_usrloc PRIVATE LibXml2::LibXml2)

target_compile_options(
  pua_usrloc
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua_usrloc/=")
set_target_properties(pua_usrloc PROPERTIES SUFFIX ".so")
