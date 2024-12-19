find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(slack SHARED ${SRC_FILES})
target_link_libraries(slack PRIVATE CURL::libcurl)

target_compile_options(
  slack PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/slack/=")
set_target_properties(slack PROPERTIES SUFFIX ".so")
