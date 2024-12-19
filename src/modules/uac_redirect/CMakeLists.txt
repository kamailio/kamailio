file(GLOB SRC_FILES "*.c")

add_library(uac_redirect SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
