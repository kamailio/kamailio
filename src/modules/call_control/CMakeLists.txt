file(GLOB SRC_FILES "*.c")

add_library(call_control SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
