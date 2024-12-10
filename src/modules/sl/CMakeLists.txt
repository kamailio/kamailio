file(GLOB SRC_FILES "*.c")

add_library(sl SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
