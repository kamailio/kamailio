file(GLOB SRC_FILES "*.c")

add_library(ss7ops SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
