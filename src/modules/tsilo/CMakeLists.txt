file(GLOB SRC_FILES "*.c")

add_library(tsilo SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
if(${FLAVOUR} STREQUAL "kamailio")
  target_compile_definitions(tsilo PRIVATE WITH_EVENT_LOCAL_REQUEST)
endif()

target_compile_options(tsilo PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/tsilo/=")
