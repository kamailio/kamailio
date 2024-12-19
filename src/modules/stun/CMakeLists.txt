file(GLOB SRC_FILES "*.c")

add_library(stun SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
