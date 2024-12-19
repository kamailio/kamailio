file(GLOB SRC_FILES "*.c")

add_library(benchmark SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  benchmark
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/benchmark/=")
set_target_properties(benchmark PROPERTIES SUFFIX ".so")
