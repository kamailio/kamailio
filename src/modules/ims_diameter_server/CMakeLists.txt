file(GLOB SRC_FILES "*.c")

add_library(ims_diameter_server SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  ims_diameter_server
  PUBLIC
    "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/ims_diameter_server/=")
set_target_properties(ims_diameter_server PROPERTIES SUFFIX ".so")
