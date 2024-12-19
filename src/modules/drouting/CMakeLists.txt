file(GLOB SRC_FILES "*.c")

add_library(drouting SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  drouting
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/drouting/=")
set_target_properties(drouting PROPERTIES SUFFIX ".so")
