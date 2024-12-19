file(GLOB SRC_FILES "*.c")

add_library(imc SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  imc PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/imc/=")
set_target_properties(imc PROPERTIES SUFFIX ".so")
