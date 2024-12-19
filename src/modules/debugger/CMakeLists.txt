file(GLOB SRC_FILES "*.c")

add_library(debugger SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
