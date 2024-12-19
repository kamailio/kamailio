file(GLOB SRC_FILES "*.c")

add_library(usrloc SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  usrloc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/usrloc/=")
set_target_properties(usrloc PROPERTIES SUFFIX ".so")
