file(GLOB SRC_FILES "*.c")

add_library(app_jsdt SHARED ${SRC_FILES})
# target_link_libraries(.*PRIVATE)
