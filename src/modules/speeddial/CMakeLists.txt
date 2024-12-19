file(GLOB SRC_FILES "*.c")

add_library(speeddial SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  speeddial
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/speeddial/=")
set_target_properties(speeddial PROPERTIES SUFFIX ".so")
