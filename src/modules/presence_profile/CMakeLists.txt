file(GLOB SRC_FILES "*.c")

add_library(presence_profile SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
