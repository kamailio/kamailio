file(GLOB topos_htable_SRC "*.c")

add_library(topos_htable SHARED ${topos_htable_SRC})

target_compile_options(
  topos_htable
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/topos_htable/=")
set_target_properties(topos_htable PROPERTIES SUFFIX ".so")
