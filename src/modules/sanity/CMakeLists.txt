file(GLOB SRC_FILES "*.c")

add_library(sanity SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sanity PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sanity/=")
set_target_properties(sanity PROPERTIES SUFFIX ".so")
