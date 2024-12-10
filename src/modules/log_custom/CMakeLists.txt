file(GLOB SRC_FILES "*.c")

add_library(log_custom SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
