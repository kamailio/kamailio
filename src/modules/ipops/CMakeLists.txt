file(GLOB SRC_FILES "*.c")

add_library(ipops SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
