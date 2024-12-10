file(GLOB SRC_FILES "*.c")

add_library(db2_ops SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
