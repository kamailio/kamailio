file(GLOB SRC_FILES "*.c")

add_library(async SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  async PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/async/=")
set_target_properties(async PROPERTIES SUFFIX ".so")
