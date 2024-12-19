file(GLOB SRC_FILES "*.c")

add_library(mqueue SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  mqueue PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/mqueue/=")
set_target_properties(mqueue PROPERTIES SUFFIX ".so")
