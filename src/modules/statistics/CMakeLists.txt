file(GLOB SRC_FILES "*.c")

add_library(statistics SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  statistics
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/statistics/=")
set_target_properties(statistics PROPERTIES SUFFIX ".so")
