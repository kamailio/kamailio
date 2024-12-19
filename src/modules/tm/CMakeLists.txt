file(GLOB SRC_FILES "*.c")

add_library(tm SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
if(${FLAVOUR} STREQUAL "kamailio")
  target_compile_definitions(tm PRIVATE WITH_EVENT_LOCAL_REQUEST)
endif()

target_compile_options(
  tm PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/tm/=")
set_target_properties(tm PROPERTIES SUFFIX ".so")
