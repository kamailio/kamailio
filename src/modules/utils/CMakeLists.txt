find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(utils SHARED ${SRC_FILES})

target_link_libraries(utils PRIVATE LibXml2::LibXml2)

target_compile_options(
  utils PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/utils/=")
set_target_properties(utils PROPERTIES SUFFIX ".so")
