file(GLOB SRC_FILES "*.c")

add_library(uid_domain SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  uid_domain
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uid_domain/=")
set_target_properties(uid_domain PROPERTIES SUFFIX ".so")
