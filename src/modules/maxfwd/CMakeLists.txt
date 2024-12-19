file(GLOB SRC_FILES "*.c")

add_library(maxfwd SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  maxfwd PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/maxfwd/=")
set_target_properties(maxfwd PROPERTIES SUFFIX ".so")
