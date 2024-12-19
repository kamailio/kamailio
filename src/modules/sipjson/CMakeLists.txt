file(GLOB SRC_FILES "*.c")

add_library(sipjson SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sipjson PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sipjson/=")
set_target_properties(sipjson PROPERTIES SUFFIX ".so")
