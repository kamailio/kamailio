file(GLOB SRC_FILES "*.c")

add_library(nosip SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  nosip PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/nosip/=")
set_target_properties(nosip PROPERTIES SUFFIX ".so")
