file(GLOB SRC_FILES "*.c")

add_library(enum SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  enum PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/enum/=")
set_target_properties(enum PROPERTIES SUFFIX ".so")
