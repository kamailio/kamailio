file(GLOB SRC_FILES "*.c")

add_library(sipt SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sipt PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sipt/=")
set_target_properties(sipt PROPERTIES SUFFIX ".so")
