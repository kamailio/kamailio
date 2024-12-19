file(GLOB SRC_FILES "*.c")

add_library(xhttp_prom SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
