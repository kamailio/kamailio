file(GLOB SRC_FILES "*.c")

add_library(pv_headers SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
