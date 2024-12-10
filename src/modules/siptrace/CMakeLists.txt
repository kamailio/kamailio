file(GLOB SRC_FILES "*.c")

add_library(siptrace SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
