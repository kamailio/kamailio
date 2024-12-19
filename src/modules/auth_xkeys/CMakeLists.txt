file(GLOB SRC_FILES "*.c")

add_library(auth_xkeys SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
