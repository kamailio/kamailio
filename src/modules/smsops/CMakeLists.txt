file(GLOB SRC_FILES "*.c")

add_library(smsops SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  smsops PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/smsops/=")
set_target_properties(smsops PROPERTIES SUFFIX ".so")
