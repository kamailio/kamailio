file(GLOB SRC_FILES "*.c")

add_library(msilo SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  msilo PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/msilo/=")
set_target_properties(msilo PROPERTIES SUFFIX ".so")
