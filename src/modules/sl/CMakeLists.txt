file(GLOB SRC_FILES "*.c")

add_library(sl SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  sl PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/sl/=")
set_target_properties(sl PROPERTIES SUFFIX ".so")
