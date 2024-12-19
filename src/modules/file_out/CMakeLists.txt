file(GLOB SRC_FILES "*.c")

add_library(file_out SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
