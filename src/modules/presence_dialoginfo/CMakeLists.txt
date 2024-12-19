find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(presence_dialoginfo SHARED ${SRC_FILES})
target_link_libraries(presence_dialoginfo PRIVATE LibXml2::LibXml2)

target_compile_options(
  presence_dialoginfo
  PUBLIC
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/presence_dialoginfo/=")
set_target_properties(presence_dialoginfo PROPERTIES SUFFIX ".so")
