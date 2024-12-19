find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(pua SHARED ${SRC_FILES})

target_link_libraries(pua PRIVATE LibXml2::LibXml2)

target_compile_options(
  pua PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pua/=")
set_target_properties(pua PROPERTIES SUFFIX ".so")
