file(GLOB SRC_FILES "*.c")

add_library(uid_uri_db SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  uid_uri_db
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uid_uri_db/=")
set_target_properties(uid_uri_db PROPERTIES SUFFIX ".so")
