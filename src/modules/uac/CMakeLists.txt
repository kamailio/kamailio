file(GLOB SRC_FILES "*.c")

add_library(uac SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  uac PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uac/=")
set_target_properties(uac PROPERTIES SUFFIX ".so")
