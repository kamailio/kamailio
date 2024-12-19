file(GLOB SRC_FILES "*.c")

add_library(siprepo SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  siprepo PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/siprepo/=")
set_target_properties(siprepo PROPERTIES SUFFIX ".so")
