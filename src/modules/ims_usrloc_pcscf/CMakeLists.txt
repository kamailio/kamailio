file(GLOB SRC_FILES "*.c")

add_library(ims_usrloc_pcscf SHARED ${SRC_FILES})

# target_link_libraries(.*PRIVATE)
