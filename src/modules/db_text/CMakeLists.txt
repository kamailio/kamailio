file(GLOB SRC_FILES "*.c")

add_library(db_text SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  db_text PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/db_text/=")
set_target_properties(db_text PROPERTIES SUFFIX ".so")
