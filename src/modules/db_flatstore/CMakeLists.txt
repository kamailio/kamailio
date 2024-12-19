file(GLOB SRC_FILES "*.c")

add_library(db_flatstore SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
