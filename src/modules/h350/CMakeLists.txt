file(GLOB SRC_FILES "*.c")

add_library(h350 SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
