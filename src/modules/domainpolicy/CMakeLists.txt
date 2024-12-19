file(GLOB SRC_FILES "*.c")

add_library(domainpolicy SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)

target_compile_options(
  domainpolicy
  PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/domainpolicy/=")
set_target_properties(domainpolicy PROPERTIES SUFFIX ".so")
