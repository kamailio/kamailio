file(GLOB SRC_FILES "*.c")

add_library(auth_diameter SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
