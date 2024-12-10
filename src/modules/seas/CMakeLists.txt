file(GLOB SRC_FILES "*.c")

add_library(seas SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
