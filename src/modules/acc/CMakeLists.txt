file(GLOB acc_SRC "*.c")

add_library(acc SHARED ${acc_SRC})

target_compile_options(
  acc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/acc/=")
set_target_properties(acc PROPERTIES SUFFIX ".so")
