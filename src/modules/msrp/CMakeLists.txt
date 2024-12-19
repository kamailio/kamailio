file(GLOB SRC_FILES "*.c")

add_library(msrp SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  msrp PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/msrp/=")
set_target_properties(msrp PROPERTIES SUFFIX ".so")
