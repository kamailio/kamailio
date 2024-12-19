file(GLOB SRC_FILES "*.c")

add_library(sworker SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sworker PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sworker/=")
set_target_properties(sworker PROPERTIES SUFFIX ".so")
