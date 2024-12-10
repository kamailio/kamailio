file(GLOB SRC_FILES "*.c")

add_library(permissions SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
