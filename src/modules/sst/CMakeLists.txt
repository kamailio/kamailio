file(GLOB SRC_FILES "*.c")

add_library(sst SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sst PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sst/=")
set_target_properties(sst PROPERTIES SUFFIX ".so")
