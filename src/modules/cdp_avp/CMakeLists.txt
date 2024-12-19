file(GLOB SRC_FILES "*.c")

add_library(cdp_avp SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
