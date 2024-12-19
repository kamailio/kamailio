file(GLOB SRC_FILES "*.c")

find_package(LibXml2 REQUIRED)

add_library(cdp SHARED ${SRC_FILES})
target_link_libraries(cdp PRIVATE LibXml2::LibXml2)

target_compile_options(
  cdp PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/cdp/=")
set_target_properties(cdp PROPERTIES SUFFIX ".so")
