file(GLOB SRC_FILES "*.c")

add_library(tmx SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
# target_compile_definitions(tmx PRIVATE STATISTICS)
