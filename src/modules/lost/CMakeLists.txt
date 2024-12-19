find_package(CURL REQUIRED)
find_package(LibXml2 REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(lost SHARED ${SRC_FILES})
target_link_libraries(lost PRIVATE CURL::libcurl LibXml2::LibXml2)

target_compile_options(
  lost PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/lost/=")
set_target_properties(lost PROPERTIES SUFFIX ".so")
