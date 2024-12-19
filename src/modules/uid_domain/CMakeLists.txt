file(GLOB SRC_FILES "*.c")

add_library(uid_domain SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
