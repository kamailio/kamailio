file(GLOB SRC_FILES "*.c")

add_library(db_flatstore SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  db_flatstore
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_flatstore/=")
set_target_properties(db_flatstore PROPERTIES SUFFIX ".so")
