file(GLOB SRC_FILES "*.c")

add_library(ims_charging SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
