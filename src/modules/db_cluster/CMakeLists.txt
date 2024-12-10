file(GLOB SRC_FILES "*.c")

add_library(db_cluster SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
