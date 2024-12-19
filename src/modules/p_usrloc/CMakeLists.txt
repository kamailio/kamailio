file(GLOB SRC_FILES "*.c")

add_library(p_usrloc SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
