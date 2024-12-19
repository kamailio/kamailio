file(GLOB SRC_FILES "*.c")

add_library(uid_gflags SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  uid_gflags
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/uid_gflags/=")
set_target_properties(uid_gflags PROPERTIES SUFFIX ".so")
