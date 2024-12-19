file(GLOB SRC_FILES "*.c")

add_library(auth_diameter SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  auth_diameter
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/auth_diameter/=")
set_target_properties(auth_diameter PROPERTIES SUFFIX ".so")
