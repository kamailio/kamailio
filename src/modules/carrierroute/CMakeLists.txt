file(GLOB SRC_FILES "*.c")

add_library(carrierroute SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  carrierroute
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/carrierroute/=")
set_target_properties(carrierroute PROPERTIES SUFFIX ".so")
