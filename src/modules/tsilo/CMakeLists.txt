file(GLOB SRC_FILES "*.c")

add_library(tsilo SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
if(${FLAVOUR} STREQUAL "kamailio")
  target_compile_definitions(tsilo PRIVATE WITH_EVENT_LOCAL_REQUEST)
endif()
