find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xcap_client SHARED ${SRC_FILES})

target_link_libraries(xcap_client PRIVATE CURL::libcurl)

target_compile_options(
  xcap_client
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/xcap_client/=")
set_target_properties(xcap_client PROPERTIES SUFFIX ".so")
