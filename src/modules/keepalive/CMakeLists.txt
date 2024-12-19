file(GLOB SRC_FILES "*.c")

add_library(keepalive SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
