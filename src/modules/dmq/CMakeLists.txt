file(GLOB SRC_FILES "*.c")

add_library(dmq SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  dmq PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/dmq/=")
set_target_properties(dmq PROPERTIES SUFFIX ".so")
