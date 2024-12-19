file(GLOB SRC_FILES "*.c")

add_library(cdp_avp SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

target_compile_options(
  cdp_avp PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/cdp_avp/=")
set_target_properties(cdp_avp PROPERTIES SUFFIX ".so")
