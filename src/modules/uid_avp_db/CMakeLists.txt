file(GLOB SRC_FILES "*.c")

add_library(uid_avp_db SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
