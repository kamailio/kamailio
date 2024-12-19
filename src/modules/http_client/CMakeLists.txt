find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(http_client SHARED ${SRC_FILES})
target_link_libraries(http_client PRIVATE CURL::libcurl)

target_compile_options(
  http_client
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/http_client/=")
set_target_properties(http_client PROPERTIES SUFFIX ".so")
