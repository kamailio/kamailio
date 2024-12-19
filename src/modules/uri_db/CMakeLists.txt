file(GLOB SRC_FILES "*.c")

add_library(uri_db SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  uri_db PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uri_db/=")
set_target_properties(uri_db PROPERTIES SUFFIX ".so")
