find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(presence_conference SHARED ${SRC_FILES})
target_link_libraries(presence_conference PRIVATE LibXml2::LibXml2)

target_compile_options(
  presence_conference
  PUBLIC
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/presence_conference/=")
set_target_properties(presence_conference PROPERTIES SUFFIX ".so")
