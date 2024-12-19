file(GLOB SRC_FILES "*.c")

add_library(rtjson SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  rtjson PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/rtjson/=")
set_target_properties(rtjson PROPERTIES SUFFIX ".so")
