file(GLOB SRC_FILES "*.c")

add_library(prefix_route SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
