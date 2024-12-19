file(GLOB SRC_FILES "*.c")

add_library(db_cluster SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  db_cluster
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_cluster/=")
set_target_properties(db_cluster PROPERTIES SUFFIX ".so")
