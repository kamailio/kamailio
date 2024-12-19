file(GLOB SRC_FILES "*.c")

add_library(pdb SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  pdb PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/pdb/=")
set_target_properties(pdb PROPERTIES SUFFIX ".so")
