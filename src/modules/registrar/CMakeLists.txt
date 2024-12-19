file(GLOB SRC_FILES "*.c")

add_library(registrar SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE) target_compile_definitions(registrar PRIVATE
# STATISTICS)

target_compile_options(
  registrar
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/registrar/=")
set_target_properties(registrar PROPERTIES SUFFIX ".so")
