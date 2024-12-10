file(GLOB SRC_FILES "*.c")

add_library(cfg_rpc SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)

# TODO: Also install kamcmd binary
